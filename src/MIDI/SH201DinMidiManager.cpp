#include "SH201DinMidiManager.h"
#include <juce_events/juce_events.h>

SH201DinMidiManager::SH201DinMidiManager()
    : juce::Thread ("SH201_DIN_Midi_Thread")
{
    midiOutStorage.resize (MIDI_OUT_FIFO_SIZE);
    startThread (juce::Thread::Priority::high);
}

SH201DinMidiManager::~SH201DinMidiManager()
{
    closeDevices();
    stopThread (2000);
}

bool SH201DinMidiManager::openDevices (const juce::String& inId, const juce::String& inName, const juce::String& outId, const juce::String& outName)
{
    bool success = true;

    // --- 1. Handle MIDI Input ---
    if (inId.isNotEmpty() || inName.isNotEmpty())
    {
        juce::MidiDeviceInfo targetIn;
        bool foundIn = false;

        for (const auto& dev : juce::MidiInput::getAvailableDevices())
        {
            if ((inId.isNotEmpty() && dev.identifier == inId) || 
                (inName.isNotEmpty() && dev.name == inName))
            {
                targetIn = dev;
                foundIn = true;
                break;
            }
        }

        if (foundIn)
        {
            if (midiInput != nullptr)
            {
                midiInput->stop();
                midiInput.reset();
            }

            midiInput = juce::MidiInput::openDevice (targetIn.identifier, this);
            if (midiInput != nullptr)
            {
                midiInput->start();
                currentInIdentifier = targetIn.identifier;
                currentInName = targetIn.name;
                juce::Logger::writeToLog ("SH201DinMidiManager: Opened MIDI In device '" + targetIn.name + "'");
            }
            else
            {
                currentInIdentifier.clear();
                currentInName.clear();
                juce::Logger::writeToLog ("SH201DinMidiManager: Failed to open MIDI In device '" + targetIn.name + "'");
                success = false;
            }
        }
        else
        {
            if (midiInput != nullptr)
            {
                midiInput->stop();
                midiInput.reset();
            }
            currentInIdentifier.clear();
            currentInName.clear();
            juce::Logger::writeToLog ("SH201DinMidiManager: MIDI In device not found: '" + (inId.isNotEmpty() ? inId : inName) + "'");
            success = false;
        }
    }
    else
    {
        if (midiInput != nullptr)
        {
            midiInput->stop();
            midiInput.reset();
        }
        currentInIdentifier.clear();
        currentInName.clear();
    }

    // --- 2. Handle MIDI Output ---
    if (outId.isNotEmpty() || outName.isNotEmpty())
    {
        juce::MidiDeviceInfo targetOut;
        bool foundOut = false;

        for (const auto& dev : juce::MidiOutput::getAvailableDevices())
        {
            if ((outId.isNotEmpty() && dev.identifier == outId) || 
                (outName.isNotEmpty() && dev.name == outName))
            {
                targetOut = dev;
                foundOut = true;
                break;
            }
        }

        if (foundOut)
        {
            std::lock_guard<std::mutex> lock (midiOutMutex);
            midiOutput = juce::MidiOutput::openDevice (targetOut.identifier);
            if (midiOutput != nullptr)
            {
                currentOutIdentifier = targetOut.identifier;
                currentOutName = targetOut.name;
                juce::Logger::writeToLog ("SH201DinMidiManager: Opened MIDI Out device '" + targetOut.name + "'");
            }
            else
            {
                currentOutIdentifier.clear();
                currentOutName.clear();
                juce::Logger::writeToLog ("SH201DinMidiManager: Failed to open MIDI Out device '" + targetOut.name + "'");
                success = false;
            }
        }
        else
        {
            std::lock_guard<std::mutex> lock (midiOutMutex);
            midiOutput.reset();
            currentOutIdentifier.clear();
            currentOutName.clear();
            juce::Logger::writeToLog ("SH201DinMidiManager: MIDI Out device not found: '" + (outId.isNotEmpty() ? outId : outName) + "'");
            success = false;
        }
    }
    else
    {
        std::lock_guard<std::mutex> lock (midiOutMutex);
        midiOutput.reset();
        currentOutIdentifier.clear();
        currentOutName.clear();
    }

    return success;
}

void SH201DinMidiManager::closeDevices()
{
    if (midiInput != nullptr)
    {
        midiInput->stop();
        midiInput.reset();
    }
    currentInIdentifier.clear();
    currentInName.clear();

    {
        std::lock_guard<std::mutex> lock (midiOutMutex);
        midiOutput.reset();
        currentOutIdentifier.clear();
        currentOutName.clear();
    }
}

bool SH201DinMidiManager::isConnected() const
{
    // Require BOTH input and output to be active to return true,
    // avoiding false positives when output is blocked by DAW on Windows
    return midiInput != nullptr && midiOutput != nullptr;
}

juce::String SH201DinMidiManager::getConnectionStatus() const
{
    if (midiInput != nullptr && midiOutput != nullptr)
    {
        return "DIN MIDI CONNECTED: In [" + currentInName + "] / Out [" + currentOutName + "]";
    }
    else if (midiInput != nullptr)
    {
        return "DIN MIDI: In [" + currentInName + "] (Out Disconnected)";
    }
    else if (midiOutput != nullptr)
    {
        return "DIN MIDI: Out [" + currentOutName + "] (In Disconnected)";
    }
    else
    {
        return "DIN MIDI DISCONNECTED - Select Ports in System Settings";
    }
}

bool SH201DinMidiManager::sendSysEx (const std::vector<uint8_t>& sysexData)
{
    if (sysexData.empty())
        return false;

    std::lock_guard<std::mutex> lock (midiOutMutex);
    if (midiOutput == nullptr)
        return false;

    midiOutput->sendMessageNow (juce::MidiMessage (sysexData.data(), static_cast<int> (sysexData.size())));
    return true;
}

bool SH201DinMidiManager::sendMidiCC (uint8_t channel, uint8_t ccNum, uint8_t value)
{
    std::lock_guard<std::mutex> lock (midiOutMutex);
    if (midiOutput == nullptr)
        return false;

    midiOutput->sendMessageNow (juce::MidiMessage::controllerEvent (channel + 1, ccNum, value));
    return true;
}

bool SH201DinMidiManager::sendMidiMessage (const juce::MidiMessage& msg)
{
    std::lock_guard<std::mutex> lock (midiOutMutex);
    if (midiOutput == nullptr)
        return false;

    midiOutput->sendMessageNow (msg);
    return true;
}

bool SH201DinMidiManager::queueMidiMessage (const juce::MidiMessage& msg) noexcept
{
    auto* rawData = msg.getRawData();
    int size = msg.getRawDataSize();
    if (size == 0 || size > 4 || midiOutput == nullptr)
        return false;

    // SysEx should use the non-RT sendSysEx path
    if (rawData[0] == 0xF0 || rawData[0] == 0xF7)
        return false;

    MidiOutPacket pkt;
    for (int i = 0; i < size; ++i)
        pkt.data[i] = rawData[i];
    pkt.size = size;

    int start1, block1, start2, block2;
    midiOutFifo.prepareToWrite (1, start1, block1, start2, block2);
    if (block1 > 0)
    {
        midiOutStorage[start1] = pkt;
        midiOutFifo.finishedWrite (1);
        notify();
        return true;
    }
    else if (block2 > 0)
    {
        midiOutStorage[start2] = pkt;
        midiOutFifo.finishedWrite (1);
        notify();
        return true;
    }

    return false;
}

bool SH201DinMidiManager::requestPatchMemory()
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

    sendSysEx (rq1System);
    juce::Timer::callAfterDelay (40,  [this, rq1Common]() { sendSysEx (rq1Common); });
    juce::Timer::callAfterDelay (80,  [this, rq1Upper]()  { sendSysEx (rq1Upper);  });
    juce::Timer::callAfterDelay (120, [this, rq1Lower]()  { sendSysEx (rq1Lower);  });
    juce::Timer::callAfterDelay (160, [this, rq1Delay]()  { sendSysEx (rq1Delay);  });
    juce::Timer::callAfterDelay (200, [this, rq1Reverb]() { sendSysEx (rq1Reverb); });

    return true;
}

void SH201DinMidiManager::handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message)
{
    juce::ignoreUnused (source);

    if (message.isSysEx())
    {
        const auto* rawData = message.getRawData();
        int size = message.getRawDataSize();
        if (rawData != nullptr && size > 0)
        {
            std::vector<uint8_t> sysexData (rawData, rawData + size);
            if (onSysExReceived)
                onSysExReceived (sysexData);
        }
    }
    else if (message.isController())
    {
        if (onMidiCCReceived)
            onMidiCCReceived (static_cast<uint8_t> (message.getChannel() - 1),
                              static_cast<uint8_t> (message.getControllerNumber()),
                              static_cast<uint8_t> (message.getControllerValue()));
    }
    else if (message.isProgramChange())
    {
        if (onProgramChangeReceived)
            onProgramChangeReceived (static_cast<uint8_t> (message.getChannel() - 1),
                                     static_cast<uint8_t> (message.getProgramChangeNumber()));
    }
}

void SH201DinMidiManager::run()
{
    while (! threadShouldExit())
    {
        wait (5);

        int numReady = midiOutFifo.getNumReady();
        if (numReady > 0)
        {
            int start1, block1, start2, block2;
            midiOutFifo.prepareToRead (numReady, start1, block1, start2, block2);

            std::lock_guard<std::mutex> lock (midiOutMutex);
            if (midiOutput != nullptr)
            {
                for (int i = 0; i < block1; ++i)
                {
                    const auto& pkt = midiOutStorage[start1 + i];
                    midiOutput->sendMessageNow (juce::MidiMessage (pkt.data, pkt.size));
                }
                for (int i = 0; i < block2; ++i)
                {
                    const auto& pkt = midiOutStorage[start2 + i];
                    midiOutput->sendMessageNow (juce::MidiMessage (pkt.data, pkt.size));
                }
            }

            midiOutFifo.finishedRead (block1 + block2);
        }
    }
}
