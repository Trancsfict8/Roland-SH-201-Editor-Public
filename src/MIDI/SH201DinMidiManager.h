#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>
#include <mutex>
#include <vector>

class SH201DinMidiManager : public juce::MidiInputCallback,
                            public juce::Thread
{
public:
    SH201DinMidiManager();
    ~SH201DinMidiManager() override;

    bool openDevices (const juce::String& inId, const juce::String& inName, const juce::String& outId, const juce::String& outName);
    void closeDevices();

    bool isConnected() const;
    bool isInputConnected() const { return midiInput != nullptr; }
    bool isOutputConnected() const { return midiOutput != nullptr; }

    juce::String getConnectedInName() const { return currentInName; }
    juce::String getConnectedOutName() const { return currentOutName; }
    juce::String getConnectionStatus() const;

    // Send SysEx to hardware via standard DIN MIDI Output
    bool sendSysEx (const std::vector<uint8_t>& sysexData);
    bool sendMidiCC (uint8_t channel, uint8_t ccNum, uint8_t value);
    bool sendMidiMessage (const juce::MidiMessage& msg);

    // Real-time safe queuing for processBlock
    bool queueMidiMessage (const juce::MidiMessage& msg) noexcept;

    // Request full patch memory dump from synth (RQ1)
    bool requestPatchMemory();

    // Callbacks for incoming hardware MIDI / SysEx
    std::function<void(const std::vector<uint8_t>&)> onSysExReceived;
    std::function<void(uint8_t channel, uint8_t ccNum, uint8_t value)> onMidiCCReceived;
    std::function<void(uint8_t channel, uint8_t program)> onProgramChangeReceived;

    // juce::MidiInputCallback
    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;

    // juce::Thread for draining real-time MIDI out queue
    void run() override;

private:
    std::unique_ptr<juce::MidiInput> midiInput;
    std::unique_ptr<juce::MidiOutput> midiOutput;

    juce::String currentInIdentifier;
    juce::String currentInName;
    juce::String currentOutIdentifier;
    juce::String currentOutName;

    std::mutex midiOutMutex;

    // Lock-free FIFO for real-time safe DAW MIDI transmission to DIN Output
    static constexpr int MIDI_OUT_FIFO_SIZE = 1024;
    juce::AbstractFifo midiOutFifo { MIDI_OUT_FIFO_SIZE };

    struct MidiOutPacket
    {
        uint8_t data[4];
        int size = 0;
    };
    std::vector<MidiOutPacket> midiOutStorage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SH201DinMidiManager)
};
