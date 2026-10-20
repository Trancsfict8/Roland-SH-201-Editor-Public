#include "../Utils/GlobalSettings.h"
#include "SH201USBManager.h"
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

SH201USBManager::SH201USBManager()
    : juce::Thread ("SH201_LibUSB_Thread")
{
    midiOutStorage.resize (MIDI_OUT_FIFO_SIZE);
    int res = libusb_init (&ctx);
    if (res != 0)
        juce::Logger::writeToLog ("SH201USBManager: Failed to initialize libusb: " + juce::String (libusb_error_name (res)));
    else
        juce::Logger::writeToLog ("SH201USBManager: libusb initialized successfully.");
}

SH201USBManager::~SH201USBManager()
{
    shutdown();
    if (ctx != nullptr)
    {
        libusb_exit (ctx);
        ctx = nullptr;
    }
}

bool SH201USBManager::initialize()
{
    if (GlobalSettings::getEditorMode() == 2)
    {
        juce::Logger::writeToLog ("SH201USBManager: Editor Mode is DIN MIDI. USB initialization skipped.");
        shutdown();
        return false;
    }

    std::lock_guard<std::mutex> lock (usbMutex);

    // If already connected, do a full close+reconnect cycle to ensure clean state
    if (connected.load())
    {
        juce::Logger::writeToLog ("SH201USBManager: Re-initializing — closing existing connection first...");
        connected.store (false);
        audioStreamingActive.store (false);

        // Cancel async transfers while the event thread is still running
        // so libusb_handle_events can process the cancellation callbacks
        cancelAllTransfers();

        // Give the event thread time to drain pending cancellation callbacks
        juce::Thread::sleep (200);

        // Now stop the event handling thread
        stopThread (2000);

        // Release interfaces and close handle
        releaseAndClose();
    }

    if (! tryConnect())
    {
        juce::Logger::writeToLog ("SH201USBManager: SH-201 not found or could not be claimed.");
        return false;
    }

    startThread (juce::Thread::Priority::high);
    return true;
}

void SH201USBManager::shutdown()
{
    {
        std::lock_guard<std::mutex> lock (usbMutex);
        if (! connected.load() && devHandle == nullptr)
            return;

        connected.store (false);
        audioStreamingActive.store (false);

        // Cancel all async transfers while the event thread can still drain them
        cancelAllTransfers();
    }

    // Give the event thread time to process cancellation callbacks (outside lock)
    juce::Thread::sleep (200);

    // Stop the event handling thread
    stopThread (2000);

    // Final cleanup under lock
    std::lock_guard<std::mutex> lock (usbMutex);
    releaseAndClose();
}

void SH201USBManager::cancelAllTransfers()
{
    // Cancel ISO transfers
    for (auto& t : isoInTransfers)
    {
        if (t.transfer != nullptr)
            libusb_cancel_transfer (t.transfer);
    }

    // Cancel bulk transfer
    if (bulkInTransfer != nullptr)
        libusb_cancel_transfer (bulkInTransfer);
}

void SH201USBManager::releaseAndClose()
{
    // Free ISO transfers (safe now — cancellation callbacks have already fired)
    for (auto& t : isoInTransfers)
    {
        if (t.transfer != nullptr)
        {
            libusb_free_transfer (t.transfer);
            t.transfer = nullptr;
        }
    }
    isoInTransfers.clear();

    // Free bulk transfer
    if (bulkInTransfer != nullptr)
    {
        libusb_free_transfer (bulkInTransfer);
        bulkInTransfer = nullptr;
    }

    if (devHandle != nullptr)
    {
        // Switch Audio interfaces back to Alt Setting 0 (idle) before releasing
        libusb_set_interface_alt_setting (devHandle, 0, 0);
        libusb_set_interface_alt_setting (devHandle, 1, 0);

        libusb_release_interface (devHandle, 0);
        libusb_release_interface (devHandle, 1);

        // Return MIDI/SysEx interface to Alt Setting 0 before releasing
        libusb_set_interface_alt_setting (devHandle, BULK_INTERFACE_NUMBER, 0);
        libusb_release_interface (devHandle, BULK_INTERFACE_NUMBER);

        libusb_close (devHandle);
        devHandle = nullptr;
        juce::Logger::writeToLog ("SH201USBManager: Device handle closed cleanly.");
    }
}

bool SH201USBManager::tryConnect()
{
    if (ctx == nullptr)
        return false;

    // First try Advanced Mode (PID: 0x00B3)
    devHandle = libusb_open_device_with_vid_pid (ctx, SH201_VID, SH201_PID_ADVANCED);
    if (devHandle != nullptr)
    {
        isAdvancedMode.store (true);
        juce::Logger::writeToLog ("SH201USBManager: Found Roland SH-201 in ADVANCED Mode (0x00B3).");
    }
    else
    {
        // Try Standard Mode (PID: 0x00AD)
        devHandle = libusb_open_device_with_vid_pid (ctx, SH201_VID, SH201_PID_STANDARD);
        if (devHandle != nullptr)
        {
            isAdvancedMode.store (false);
            juce::Logger::writeToLog ("SH201USBManager: Found Roland SH-201 in STANDARD Mode (0x00AD).");
        }
        else
        {
            return false;
        }
    }

    // Attempt to set configuration
    int res = libusb_set_configuration (devHandle, 1);
    if (res != 0 && res != LIBUSB_ERROR_BUSY)
    {
        juce::Logger::writeToLog ("SH201USBManager: Warning setting config 1: " + juce::String (libusb_error_name (res)));
    }

    // Claim the Bulk MIDI/SysEx Interface
    res = libusb_claim_interface (devHandle, BULK_INTERFACE_NUMBER);
    if (res != 0)
    {
        juce::Logger::writeToLog ("SH201USBManager: Bulk Interface busy or stuck (" +
                                  juce::String (libusb_error_name (res)) + "). Attempting USB reset and retry...");
        libusb_reset_device (devHandle);
        juce::Thread::sleep (300);
        res = libusb_claim_interface (devHandle, BULK_INTERFACE_NUMBER);
        if (res != 0)
        {
            juce::Logger::writeToLog ("SH201USBManager: Failed to claim Bulk Interface 2 after reset: " + juce::String (libusb_error_name (res)));
            libusb_close (devHandle);
            devHandle = nullptr;
            return false;
        }
    }

    // Explicitly set Interface 2 to Alternate Setting 0 (Bulk OUT 0x03 + Bulk IN 0x84)
    libusb_set_interface_alt_setting (devHandle, BULK_INTERFACE_NUMBER, 0);

    // Always attempt to claim Audio Interfaces 0 and 1 and set Alternate Setting 1 for Isochronous streaming
    // Unless we are in "MIDI only" Editor Mode (mode 1)
    bool audioClaimed = false;
    
    if (GlobalSettings::getEditorMode() == 0)
    {
        int res0 = libusb_claim_interface (devHandle, 0);
        int res1 = libusb_claim_interface (devHandle, 1);
        if (res0 != 0 || res1 != 0)
        {
            juce::Logger::writeToLog ("SH201USBManager: Audio interfaces busy, attempting alt 0 reset before reclaim...");
            libusb_set_interface_alt_setting (devHandle, 0, 0);
            libusb_set_interface_alt_setting (devHandle, 1, 0);
            if (res0 != 0) res0 = libusb_claim_interface (devHandle, 0);
            if (res1 != 0) res1 = libusb_claim_interface (devHandle, 1);
        }
        if (res0 == 0 && res1 == 0)
        {
            int altRes0 = libusb_set_interface_alt_setting (devHandle, 0, 1);
            int altRes1 = libusb_set_interface_alt_setting (devHandle, 1, 1);
            if (altRes0 == 0 && altRes1 == 0)
            {
                audioClaimed = true;
                juce::Logger::writeToLog ("SH201USBManager: Successfully claimed Audio Interfaces 0 and 1 (Alt Setting 1).");
            }
            else
            {
                juce::Logger::writeToLog ("SH201USBManager: Failed setting Alt Setting 1 on Audio Interfaces.");
            }
        }
        else
        {
            juce::Logger::writeToLog ("SH201USBManager: Audio Interfaces 0/1 not claimed (" +
                                      juce::String (libusb_error_name (res0)) + "/" +
                                      juce::String (libusb_error_name (res1)) + ").");
        }
    }
    else
    {
        juce::Logger::writeToLog ("SH201USBManager: Editor Mode is MIDI only. Skipping Audio Interface claim.");
        // Ensure audio interfaces are not left in an active state
        libusb_set_interface_alt_setting (devHandle, 0, 0);
        libusb_set_interface_alt_setting (devHandle, 1, 0);
    }
    audioStreamingActive.store (audioClaimed);

    connected.store (true);
    juce::Logger::writeToLog ("SH201USBManager: Successfully claimed Interface 2 (Bulk MIDI/SysEx).");

    startBulkInListener();
    if (audioClaimed)
        startIsoInListener();
    return true;
}

void SH201USBManager::startBulkInListener()
{
    bulkInBuffer.resize (512, 0);
    bulkInTransfer = libusb_alloc_transfer (0);
    if (bulkInTransfer == nullptr)
    {
        juce::Logger::writeToLog ("SH201USBManager: Failed to allocate bulk in transfer.");
        return;
    }

    libusb_fill_bulk_transfer (bulkInTransfer, devHandle, ENDPOINT_BULK_IN,
                               bulkInBuffer.data(), static_cast<int> (bulkInBuffer.size()),
                               bulkInCallback, this, 0); // 0 timeout = infinite wait

    int res = libusb_submit_transfer (bulkInTransfer);
    if (res != 0)
        juce::Logger::writeToLog ("SH201USBManager: Failed to submit bulk in transfer: " + juce::String (libusb_error_name (res)));
    else
        juce::Logger::writeToLog ("SH201USBManager: Asynchronous Bulk IN listener started.");
}

void LIBUSB_CALL SH201USBManager::bulkInCallback (struct libusb_transfer* transfer)
{
    auto* manager = static_cast<SH201USBManager*> (transfer->user_data);
    if (transfer->status == LIBUSB_TRANSFER_COMPLETED && transfer->actual_length > 0)
    {
        manager->handleBulkInPacket (transfer->buffer, transfer->actual_length);
    }

    if (manager->isConnected() && transfer->status != LIBUSB_TRANSFER_CANCELLED && transfer->status != LIBUSB_TRANSFER_NO_DEVICE)
    {
        if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
        {
            // USB endpoint error/stall — log once and delay briefly to prevent infinite interrupt storm
            juce::Logger::writeToLog ("SH201USBManager: Bulk IN transfer error (status " + juce::String (transfer->status) + ") — backing off 100ms.");
            juce::Thread::sleep (100);
        }
        libusb_submit_transfer (transfer);
    }
}

void SH201USBManager::handleBulkInPacket (const uint8_t* data, int length)
{
    // Process standard 4-byte USB-MIDI Event Packets
    for (int i = 0; i < length; i += 4)
    {
        const uint8_t* packet = data + i;
        uint8_t header = packet[0];
        uint8_t cin = header & 0x0F;

        // Diagnostic log: ignore SysEx data bytes (0x04) and real-time (0x0F)
        if (cin != 0x04 && cin != 0x0F)
        {
            juce::String hex;
            hex << juce::String::toHexString(packet[0]).paddedLeft('0', 2) << " "
                << juce::String::toHexString(packet[1]).paddedLeft('0', 2) << " "
                << juce::String::toHexString(packet[2]).paddedLeft('0', 2) << " "
                << juce::String::toHexString(packet[3]).paddedLeft('0', 2);
            juce::Logger::writeToLog("SH201USBManager - USB-MIDI RX: " + hex);
        }

        uint8_t b1 = data[i + 1];
        uint8_t b2 = data[i + 2];
        uint8_t b3 = data[i + 3];

        if (cin == 0x04) // SysEx Start or Continue (3 bytes)
        {
            incomingSysExBuffer.push_back (b1);
            incomingSysExBuffer.push_back (b2);
            incomingSysExBuffer.push_back (b3);
        }
        else if (cin == 0x05) // SysEx Ends with 1 byte
        {
            incomingSysExBuffer.push_back (b1);
            if (! incomingSysExBuffer.empty() && incomingSysExBuffer.front() == 0xF0 && incomingSysExBuffer.back() == 0xF7)
            {
                if (onSysExReceived)
                    onSysExReceived (incomingSysExBuffer);
            }
            incomingSysExBuffer.clear();
        }
        else if (cin == 0x06) // SysEx Ends with 2 bytes
        {
            incomingSysExBuffer.push_back (b1);
            incomingSysExBuffer.push_back (b2);
            if (! incomingSysExBuffer.empty() && incomingSysExBuffer.front() == 0xF0 && incomingSysExBuffer.back() == 0xF7)
            {
                if (onSysExReceived)
                    onSysExReceived (incomingSysExBuffer);
            }
            incomingSysExBuffer.clear();
        }
        else if (cin == 0x07) // SysEx Ends with 3 bytes
        {
            incomingSysExBuffer.push_back (b1);
            incomingSysExBuffer.push_back (b2);
            incomingSysExBuffer.push_back (b3);
            if (! incomingSysExBuffer.empty() && incomingSysExBuffer.front() == 0xF0 && incomingSysExBuffer.back() == 0xF7)
            {
                if (onSysExReceived)
                    onSysExReceived (incomingSysExBuffer);
            }
            incomingSysExBuffer.clear();
        }
        else if (cin == 0x0B) // MIDI Control Change (3 bytes)
        {
            uint8_t channel = b1 & 0x0F;
            uint8_t ccNum   = b2;
            uint8_t ccVal   = b3;
            if (onMidiCCReceived)
                onMidiCCReceived (channel, ccNum, ccVal);
        }
        else if (cin == 0x0C) // MIDI Program Change (2 bytes)
        {
            uint8_t channel = b1 & 0x0F;
            uint8_t program = b2;
            if (onProgramChangeReceived)
                onProgramChangeReceived (channel, program);
        }
    }
}

void SH201USBManager::closeDevice()
{
    stopIsoInListener();

    if (bulkInTransfer != nullptr)
    {
        libusb_cancel_transfer (bulkInTransfer);
        libusb_free_transfer (bulkInTransfer);
        bulkInTransfer = nullptr;
    }

    if (devHandle != nullptr)
    {
        // Explicitly switch Audio interfaces back to Alt Setting 0 (idle, stop streaming) before releasing
        libusb_set_interface_alt_setting (devHandle, 0, 0);
        libusb_set_interface_alt_setting (devHandle, 1, 0);

        if (isAdvancedMode.load())
        {
            libusb_release_interface (devHandle, 0);
            libusb_release_interface (devHandle, 1);
        }

        // Return MIDI/SysEx interface to Alt Setting 0 before releasing
        libusb_set_interface_alt_setting (devHandle, BULK_INTERFACE_NUMBER, 0);
        libusb_release_interface (devHandle, BULK_INTERFACE_NUMBER);
        libusb_close (devHandle);
        devHandle = nullptr;
    }
    connected.store (false);
    audioStreamingActive.store (false);
}

juce::String SH201USBManager::getConnectionMode() const
{
    if (! connected.load())
        return "Disconnected";

    juce::String pidStr = isAdvancedMode.load() ? "SH-201 (0x00B3)" : "SH-201 (0x00AD)";
    return pidStr + (audioStreamingActive.load() ? " - MIDI + Audio"
                                                 : " - MIDI Only");
}

bool SH201USBManager::sendSysEx(const std::vector<uint8_t>& sysexData)
{
    std::lock_guard<std::mutex> lock (usbMutex);
    if (! connected.load() || devHandle == nullptr || sysexData.empty())
        return false;

    // Convert raw SysEx sequence to 4-byte USB-MIDI Event Packets
    std::vector<uint8_t> usbPackets;
    size_t i = 0;
    while (i < sysexData.size())
    {
        size_t remaining = sysexData.size() - i;
        uint8_t b0 = 0, b1 = 0, b2 = 0, b3 = 0;

        if (remaining >= 3 && sysexData[i + 2] != 0xF7)
        {
            b0 = 0x04;
            b1 = sysexData[i++];
            b2 = sysexData[i++];
            b3 = sysexData[i++];
        }
        else if (remaining == 1)
        {
            b0 = 0x05;
            b1 = sysexData[i++];
        }
        else if (remaining == 2)
        {
            b0 = 0x06;
            b1 = sysexData[i++];
            b2 = sysexData[i++];
        }
        else
        {
            b0 = 0x07;
            b1 = sysexData[i++];
            b2 = sysexData[i++];
            b3 = sysexData[i++];
        }

        usbPackets.push_back (b0);
        usbPackets.push_back (b1);
        usbPackets.push_back (b2);
        usbPackets.push_back (b3);
    }

    int transferred = 0;
    int res = libusb_bulk_transfer (devHandle, ENDPOINT_BULK_OUT, usbPackets.data(),
                                    static_cast<int> (usbPackets.size()), &transferred, 1000);
    if (res != 0)
    {
        juce::Logger::writeToLog ("SH201USBManager: Bulk OUT transfer error: " + juce::String (libusb_error_name (res)));
        return false;
    }

    return true;
}

bool SH201USBManager::sendMidiCC (uint8_t channel, uint8_t ccNum, uint8_t value)
{
    std::lock_guard<std::mutex> lock (usbMutex);
    if (! connected.load() || devHandle == nullptr)
        return false;

    uint8_t status = 0xB0 | (channel & 0x0F);
    uint8_t packet[4] = { 0x0B, status, ccNum, value };

    int transferred = 0;
    int res = libusb_bulk_transfer (devHandle, ENDPOINT_BULK_OUT, packet, 4, &transferred, 500);
    if (res != 0)
    {
        juce::Logger::writeToLog ("SH201USBManager: sendMidiCC transfer error: " + juce::String (libusb_error_name (res)));
        return false;
    }
    return true;
}

bool SH201USBManager::sendMidiMessage (const juce::MidiMessage& msg)
{
    std::lock_guard<std::mutex> lock (usbMutex);
    if (! connected.load() || devHandle == nullptr)
        return false;

    auto* rawData = msg.getRawData();
    int size = msg.getRawDataSize();
    if (size == 0)
        return false;

    uint8_t status = rawData[0];
    uint8_t highNibble = status >> 4;

    if (status == 0xF0 || status == 0xF7)
        return sendSysEx (std::vector<uint8_t> (rawData, rawData + size));

    uint8_t cin = 0x00;
    if (highNibble >= 0x8 && highNibble <= 0xE)
        cin = highNibble;
    else if (status >= 0xF8) // System Real-Time (0xF8 Clock, 0xFA Start, 0xFB Continue, 0xFC Stop, etc.)
        cin = 0x0F;
    else if (status == 0xF1 || status == 0xF3) // 2-byte System Common
        cin = 0x02;
    else if (status == 0xF2) // 3-byte System Common
        cin = 0x03;
    else if (status == 0xF6) // 1-byte System Common
        cin = 0x05;
    else
        return false;

    uint8_t b1 = rawData[0];
    uint8_t b2 = size > 1 ? rawData[1] : 0;
    uint8_t b3 = size > 2 ? rawData[2] : 0;

    uint8_t packet[4] = { cin, b1, b2, b3 };

    int transferred = 0;
    int res = libusb_bulk_transfer (devHandle, ENDPOINT_BULK_OUT, packet, 4, &transferred, 20);
    return res == 0;
}

bool SH201USBManager::queueMidiMessageForUsb (const juce::MidiMessage& msg) noexcept
{
    // Real-time safe: no locks, no allocations, no kernel calls
    auto* rawData = msg.getRawData();
    int size = msg.getRawDataSize();
    if (size == 0 || ! connected.load (std::memory_order_relaxed))
        return false;

    uint8_t status = rawData[0];
    uint8_t highNibble = status >> 4;

    // SysEx should use the non-RT sendSysEx path, not this queue
    if (status == 0xF0 || status == 0xF7)
        return false;

    uint8_t cin = 0x00;
    if (highNibble >= 0x8 && highNibble <= 0xE)
        cin = highNibble;
    else if (status >= 0xF8) // System Real-Time (0xF8 Clock, 0xFA Start, 0xFB Continue, 0xFC Stop, etc.)
        cin = 0x05; // USB MIDI 1.0 Spec Table 4-1: Single-byte System Real-Time is CIN 0x05
    else if (status == 0xF1 || status == 0xF3) // 2-byte System Common
        cin = 0x02;
    else if (status == 0xF2) // 3-byte System Common
        cin = 0x03;
    else if (status == 0xF6) // 1-byte System Common
        cin = 0x05;
    else
        return false;

    MidiOutPacket pkt;
    pkt.data[0] = cin;
    pkt.data[1] = rawData[0];
    pkt.data[2] = size > 1 ? rawData[1] : 0;
    pkt.data[3] = size > 2 ? rawData[2] : 0;

    int start1, block1, start2, block2;
    midiOutFifo.prepareToWrite (1, start1, block1, start2, block2);
    if (block1 > 0)
    {
        midiOutStorage[start1] = pkt;
        midiOutFifo.finishedWrite (1);
        return true;
    }
    // FIFO full — drop the message rather than block the audio thread
    return false;
}

bool SH201USBManager::requestPatchMemory()
{
    // Roland RQ1 (Request Data 1) Commands for System Common, Patch Common, Tone 1 Upper, Tone 2 Lower
    std::vector<uint8_t> rq1System = {
        0xF0, 0x41, 0x10, 0x00, 0x00, 0x16, 0x11,
        0x01, 0x00, 0x00, 0x00,  // Address: System Common
        0x00, 0x00, 0x00, 0x25,  // Size: 0x25 (37 bytes)
        0x5A,                    // Checksum
        0xF7
    };

    std::vector<uint8_t> rq1Common = {
        0xF0, 0x41, 0x10, 0x00, 0x00, 0x16, 0x11,
        0x10, 0x00, 0x00, 0x00,  // Address: Temporary Patch Common
        0x00, 0x00, 0x00, 0x21,  // Size: 0x21 (33 bytes)
        0x4F,                    // Checksum
        0xF7
    };

    std::vector<uint8_t> rq1Upper = {
        0xF0, 0x41, 0x10, 0x00, 0x00, 0x16, 0x11,
        0x10, 0x00, 0x01, 0x00,  // Address: Temporary Patch Tone 1 (Upper)
        0x00, 0x00, 0x00, 0x40,  // Size: 0x40 (64 bytes)
        0x2F,                    // Checksum
        0xF7
    };

    std::vector<uint8_t> rq1Lower = {
        0xF0, 0x41, 0x10, 0x00, 0x00, 0x16, 0x11,
        0x10, 0x00, 0x02, 0x00,  // Address: Temporary Patch Tone 2 (Lower)
        0x00, 0x00, 0x00, 0x40,  // Size: 0x40 (64 bytes)
        0x2E,                    // Checksum
        0xF7
    };

    std::vector<uint8_t> rq1Delay = {
        0xF0, 0x41, 0x10, 0x00, 0x00, 0x16, 0x11,
        0x10, 0x00, 0x03, 0x00,  // Address: Temporary Patch Delay
        0x00, 0x00, 0x00, 0x05,  // Size: 0x05 (5 bytes)
        0x68,                    // Checksum
        0xF7
    };

    std::vector<uint8_t> rq1Reverb = {
        0xF0, 0x41, 0x10, 0x00, 0x00, 0x16, 0x11,
        0x10, 0x00, 0x04, 0x00,  // Address: Temporary Patch Reverb
        0x00, 0x00, 0x00, 0x0A,  // Size: 0x0A (10 bytes)
        0x62,                    // Checksum
        0xF7
    };

    sendSysEx(rq1System);
    juce::Timer::callAfterDelay(40, [this, rq1Common]() { sendSysEx(rq1Common); });
    juce::Timer::callAfterDelay(80, [this, rq1Upper]() { sendSysEx(rq1Upper); });
    juce::Timer::callAfterDelay(120, [this, rq1Lower]() { sendSysEx(rq1Lower); });
    juce::Timer::callAfterDelay(160, [this, rq1Delay]() { sendSysEx(rq1Delay); });
    juce::Timer::callAfterDelay(200, [this, rq1Reverb]() { sendSysEx(rq1Reverb); });

    return true;
}

void SH201USBManager::run()
{
    while (! threadShouldExit())
    {
        if (ctx != nullptr)
        {
            struct timeval tv = { 0, 10000 }; // 10ms timeout for tighter MIDI out latency
            libusb_handle_events_timeout_completed (ctx, &tv, nullptr);
        }
        else
        {
            juce::Thread::sleep (5);
        }

        // Drain the lock-free MIDI out queue (fed by processBlock)
        drainMidiOutQueue();
    }
}

void SH201USBManager::drainMidiOutQueue()
{
    int numReady = midiOutFifo.getNumReady();
    if (numReady == 0)
        return;

    // Batch up to 64 packets per drain pass to keep USB transfers efficient
    int toDrain = juce::jmin (numReady, 64);

    int start1, block1, start2, block2;
    midiOutFifo.prepareToRead (toDrain, start1, block1, start2, block2);

    // Build a contiguous USB packet buffer for bulk transfer
    std::vector<uint8_t> usbBuf;
    usbBuf.reserve ((block1 + block2) * 4);

    for (int i = 0; i < block1; ++i)
    {
        auto& pkt = midiOutStorage[start1 + i];
        usbBuf.push_back (pkt.data[0]);
        usbBuf.push_back (pkt.data[1]);
        usbBuf.push_back (pkt.data[2]);
        usbBuf.push_back (pkt.data[3]);
    }
    for (int i = 0; i < block2; ++i)
    {
        auto& pkt = midiOutStorage[start2 + i];
        usbBuf.push_back (pkt.data[0]);
        usbBuf.push_back (pkt.data[1]);
        usbBuf.push_back (pkt.data[2]);
        usbBuf.push_back (pkt.data[3]);
    }

    midiOutFifo.finishedRead (block1 + block2);

    if (! usbBuf.empty())
    {
        std::lock_guard<std::mutex> lock (usbMutex);
        if (connected.load() && devHandle != nullptr)
        {
            int transferred = 0;
            libusb_bulk_transfer (devHandle, ENDPOINT_BULK_OUT, usbBuf.data(),
                                  static_cast<int> (usbBuf.size()), &transferred, 20);
        }
    }
}

void SH201USBManager::startIsoInListener()
{
    audioInFifo.reset();
    audioInStorageL.resize (32768, 0.0f);
    audioInStorageR.resize (32768, 0.0f);

    isoInTransfers.resize (NUM_ISO_TRANSFERS);
    for (int i = 0; i < NUM_ISO_TRANSFERS; ++i)
    {
        auto& t = isoInTransfers[i];
        t.buffer.resize (ISO_TRANSFER_SIZE, 0);
        t.transfer = libusb_alloc_transfer (ISO_NUM_PACKETS);
        if (t.transfer == nullptr)
            continue;

        libusb_fill_iso_transfer (t.transfer, devHandle, ENDPOINT_ISO_IN,
                                  t.buffer.data(), ISO_TRANSFER_SIZE, ISO_NUM_PACKETS,
                                  isoInCallback, this, 1000);
        libusb_set_iso_packet_lengths (t.transfer, ISO_PACKET_SIZE);

        int res = libusb_submit_transfer (t.transfer);
        if (res != 0)
            juce::Logger::writeToLog ("SH201USBManager: Failed submitting ISO transfer " + juce::String (i) + ": " + juce::String (libusb_error_name (res)));
    }
    juce::Logger::writeToLog ("SH201USBManager: Started Isochronous IN Audio stream (" + juce::String (NUM_ISO_TRANSFERS) + " transfers).");
}

void SH201USBManager::stopIsoInListener()
{
    for (auto& t : isoInTransfers)
    {
        if (t.transfer != nullptr)
        {
            libusb_cancel_transfer (t.transfer);
            libusb_free_transfer (t.transfer);
            t.transfer = nullptr;
        }
    }
    isoInTransfers.clear();
}

void LIBUSB_CALL SH201USBManager::isoInCallback (struct libusb_transfer* transfer)
{
    auto* manager = static_cast<SH201USBManager*> (transfer->user_data);
    if (transfer->status == LIBUSB_TRANSFER_COMPLETED)
    {
        for (int i = 0; i < transfer->num_iso_packets; ++i)
        {
            auto* desc = &transfer->iso_packet_desc[i];
            if (desc->status == LIBUSB_TRANSFER_COMPLETED && desc->actual_length > 0)
            {
                uint8_t* pktData = libusb_get_iso_packet_buffer_simple (transfer, i);
                int numFrames = desc->actual_length / 6; // 3 bytes L + 3 bytes R (24-bit stereo PCM)
                if (numFrames > 0 && manager->audioStorageReady())
                {
                    int start1, block1, start2, block2;
                    manager->audioInFifo.prepareToWrite (numFrames, start1, block1, start2, block2);

                    int inIdx = 0;
                    for (int j = 0; j < block1; ++j)
                    {
                        int32_t rawL = (pktData[inIdx*6 + 0]) | (pktData[inIdx*6 + 1] << 8) | (((int8_t)pktData[inIdx*6 + 2]) << 16);
                        int32_t rawR = (pktData[inIdx*6 + 3]) | (pktData[inIdx*6 + 4] << 8) | (((int8_t)pktData[inIdx*6 + 5]) << 16);
                        manager->audioInStorageL[start1 + j] = static_cast<float> (rawL) / 8388608.0f;
                        manager->audioInStorageR[start1 + j] = static_cast<float> (rawR) / 8388608.0f;
                        inIdx++;
                    }
                    for (int j = 0; j < block2; ++j)
                    {
                        int32_t rawL = (pktData[inIdx*6 + 0]) | (pktData[inIdx*6 + 1] << 8) | (((int8_t)pktData[inIdx*6 + 2]) << 16);
                        int32_t rawR = (pktData[inIdx*6 + 3]) | (pktData[inIdx*6 + 4] << 8) | (((int8_t)pktData[inIdx*6 + 5]) << 16);
                        manager->audioInStorageL[start2 + j] = static_cast<float> (rawL) / 8388608.0f;
                        manager->audioInStorageR[start2 + j] = static_cast<float> (rawR) / 8388608.0f;
                        inIdx++;
                    }

                    manager->audioInFifo.finishedWrite (block1 + block2);
                }
            }
        }
    }

    if (manager->isAudioStreaming() && transfer->status != LIBUSB_TRANSFER_CANCELLED && transfer->status != LIBUSB_TRANSFER_NO_DEVICE)
    {
        if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
        {
            // USB endpoint error/stall — log once and delay briefly to prevent infinite interrupt storm
            juce::Logger::writeToLog ("SH201USBManager: ISO IN transfer error (status " + juce::String (transfer->status) + ") — backing off 100ms.");
            juce::Thread::sleep (100);
        }
        libusb_submit_transfer (transfer);
    }
}

void SH201USBManager::readAudioFromUSB (juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (! audioStreamingActive.load() || ! audioStorageReady())
    {
        buffer.clear();
        return;
    }

    int readyFrames = audioInFifo.getNumReady();

    // Prevent latency buildup while avoiding audio scratchiness:
    // Windows USB transfers arrive in 8ms batches (~353 samples). A tight clamp chops packets.
    // We use hysteresis: only discard excess samples if backlog exceeds ~70ms (3072 samples),
    // and trim it to ~35ms (1536 samples) of clean jitter buffer.
    const int maxAllowedFrames = 3072;
    const int targetBufferFrames = 1536;
    if (readyFrames > maxAllowedFrames)
    {
        int excess = readyFrames - targetBufferFrames;
        int s1, b1, s2, b2;
        audioInFifo.prepareToRead (excess, s1, b1, s2, b2);
        audioInFifo.finishedRead (b1 + b2);
        readyFrames = audioInFifo.getNumReady();
    }

    double speedRatio = 44100.0 / dawSampleRate.load();
    bool needsResampling = std::abs (speedRatio - 1.0) > 0.001;

    int hardwareFramesNeeded = needsResampling ? (int)std::ceil(numSamples * speedRatio) + 4 : numSamples;
    int framesToRead = juce::jmin (hardwareFramesNeeded, readyFrames);

    if (framesToRead == 0)
    {
        buffer.clear();
        return;
    }

    int start1, block1, start2, block2;
    audioInFifo.prepareToRead (framesToRead, start1, block1, start2, block2);

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    if (! needsResampling)
    {
        if (block1 > 0)
        {
            juce::FloatVectorOperations::copy (left, &audioInStorageL[start1], block1);
            if (right) juce::FloatVectorOperations::copy (right, &audioInStorageR[start1], block1);
        }
        if (block2 > 0)
        {
            juce::FloatVectorOperations::copy (left + block1, &audioInStorageL[start2], block2);
            if (right) juce::FloatVectorOperations::copy (right + block1, &audioInStorageR[start2], block2);
        }

        audioInFifo.finishedRead (block1 + block2);

        if (framesToRead < numSamples)
        {
            buffer.clear (0, framesToRead, numSamples - framesToRead);
            if (right)
                buffer.clear (1, framesToRead, numSamples - framesToRead);
        }
    }
    else
    {
        auto* tempL = tempInputBuffer.getWritePointer(0);
        auto* tempR = tempInputBuffer.getWritePointer(1);

        if (block1 > 0)
        {
            juce::FloatVectorOperations::copy (tempL, &audioInStorageL[start1], block1);
            juce::FloatVectorOperations::copy (tempR, &audioInStorageR[start1], block1);
        }
        if (block2 > 0)
        {
            juce::FloatVectorOperations::copy (tempL + block1, &audioInStorageL[start2], block2);
            juce::FloatVectorOperations::copy (tempR + block1, &audioInStorageR[start2], block2);
        }

        int totalRead = block1 + block2;
        int safeOutputSamples = numSamples;
        
        // Prevent out-of-bounds reading by the interpolator if we don't have enough input samples
        if (totalRead < hardwareFramesNeeded)
        {
            safeOutputSamples = juce::jmax (0, (int)((totalRead - 4) / speedRatio));
            safeOutputSamples = juce::jmin (safeOutputSamples, numSamples);
        }

        int consumedL = 0;
        int consumedR = 0;

        if (safeOutputSamples > 0)
        {
            consumedL = interpolatorL.process (speedRatio, tempInputBuffer.getReadPointer(0), left, safeOutputSamples);
            if (right)
                consumedR = interpolatorR.process (speedRatio, tempInputBuffer.getReadPointer(1), right, safeOutputSamples);
            else
                consumedR = consumedL;
        }

        audioInFifo.finishedRead (juce::jmax (consumedL, consumedR));

        if (safeOutputSamples < numSamples)
        {
            buffer.clear (0, safeOutputSamples, numSamples - safeOutputSamples);
            if (right)
                buffer.clear (1, safeOutputSamples, numSamples - safeOutputSamples);
        }
    }
}
