#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#if JUCE_WINDOWS
#include <winsock2.h>
#include <windows.h>
#endif

#include <libusb.h>
#include <atomic>
#include <mutex>
#include <vector>

class SH201USBManager : public juce::Thread
{
public:
    SH201USBManager();
    ~SH201USBManager() override;

    bool initialize();
    void shutdown();

    bool isConnected() const { return connected.load(); }
    juce::String getConnectionMode() const;
    bool isAudioStreaming() const { return audioStreamingActive.load(); }
    bool audioStorageReady() const { return !audioInStorageL.empty() && !audioInStorageR.empty(); }
    void setDawSampleRate (double newRate) { dawSampleRate.store (newRate); }
    void readAudioFromUSB (juce::AudioBuffer<float>& buffer, int numSamples);

    // Send raw SysEx (DT1 or RQ1) to the SH-201 via Bulk Endpoint 0x03
    bool sendSysEx(const std::vector<uint8_t>& sysexData);
    bool sendMidiCC (uint8_t channel, uint8_t ccNum, uint8_t value);
    bool sendMidiMessage (const juce::MidiMessage& msg);

    // Real-time safe method for processBlock - pushes to lock-free FIFO (no locks, no kernel calls)
    bool queueMidiMessageForUsb (const juce::MidiMessage& msg) noexcept;

    // Request the current patch memory dump from the synth (RQ1)
    bool requestPatchMemory();

    // Set callback for receiving MIDI / SysEx messages from hardware
    std::function<void(const std::vector<uint8_t>&)> onSysExReceived;
    std::function<void(uint8_t channel, uint8_t ccNum, uint8_t value)> onMidiCCReceived;
    std::function<void(uint8_t channel, uint8_t program)> onProgramChangeReceived;

    // Thread function for libusb event processing
    void run() override;

private:
    bool tryConnect();
    void closeDevice();
    void cancelAllTransfers();
    void releaseAndClose();
    void startBulkInListener();
    void handleBulkInPacket (const uint8_t* data, int length);
    static void LIBUSB_CALL bulkInCallback (struct libusb_transfer* transfer);

    void startIsoInListener();
    void stopIsoInListener();
    static void LIBUSB_CALL isoInCallback (struct libusb_transfer* transfer);

    // Drain the lock-free MIDI out queue and transmit via USB
    void drainMidiOutQueue();

    libusb_context* ctx = nullptr;
    libusb_device_handle* devHandle = nullptr;
    libusb_transfer* bulkInTransfer = nullptr;
    
    std::vector<uint8_t> bulkInBuffer;
    std::vector<uint8_t> incomingSysExBuffer;

    // Isochronous Audio Pool (4 concurrent transfers)
    static constexpr int NUM_ISO_TRANSFERS = 4;
    static constexpr int ISO_PACKET_SIZE = 288;
    static constexpr int ISO_NUM_PACKETS = 8;
    static constexpr int ISO_TRANSFER_SIZE = ISO_PACKET_SIZE * ISO_NUM_PACKETS;

    struct IsoTransferData {
        struct libusb_transfer* transfer = nullptr;
        std::vector<uint8_t> buffer;
    };
    std::vector<IsoTransferData> isoInTransfers;

    // Lock-free Stereo Audio FIFO (32768 frames ~0.74 seconds at 44.1kHz)
    juce::AbstractFifo audioInFifo { 32768 };
    std::vector<float> audioInStorageL;
    std::vector<float> audioInStorageR;

    // Resampling interpolators and DAW sample rate
    std::atomic<double> dawSampleRate { 44100.0 };
    juce::LagrangeInterpolator interpolatorL;
    juce::LagrangeInterpolator interpolatorR;
    juce::AudioBuffer<float> tempInputBuffer { 2, 16384 };

    // Lock-free MIDI Out FIFO for real-time safe processBlock
    // Each entry is a 4-byte USB-MIDI event packet
    static constexpr int MIDI_OUT_FIFO_SIZE = 1024;
    juce::AbstractFifo midiOutFifo { MIDI_OUT_FIFO_SIZE };
    struct MidiOutPacket { uint8_t data[4]; };
    std::vector<MidiOutPacket> midiOutStorage;

    std::atomic<bool> connected { false };
    std::atomic<bool> isAdvancedMode { false };
    std::atomic<bool> audioStreamingActive { false };
    
    std::mutex usbMutex;

    static constexpr uint16_t SH201_VID = 0x0582;
    static constexpr uint16_t SH201_PID_ADVANCED = 0x00B3;
    static constexpr uint16_t SH201_PID_STANDARD = 0x00AD;

    static constexpr uint8_t ENDPOINT_BULK_OUT = 0x03;
    static constexpr uint8_t ENDPOINT_BULK_IN  = 0x84;
    static constexpr uint8_t ENDPOINT_ISO_OUT  = 0x01;
    static constexpr uint8_t ENDPOINT_ISO_IN   = 0x82;
    static constexpr int BULK_INTERFACE_NUMBER = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SH201USBManager)
};
