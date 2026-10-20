#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <unordered_map>
#include "USB/SH201USBManager.h"
#include "MIDI/SH201DinMidiManager.h"

class SH201AudioProcessor : public juce::AudioProcessor,
                            public juce::Timer
{
public:
    JUCE_DECLARE_WEAK_REFERENCEABLE (SH201AudioProcessor)
    SH201AudioProcessor();
    ~SH201AudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Public Hardware Managers & Communication Router
    SH201USBManager& getUSBManager() { return *usbManager; }
    SH201DinMidiManager& getDinMidiManager() { return *dinMidiManager; }

    bool isHardwareConnected() const;
    juce::String getHardwareConnectionStatus() const;
    bool isAudioStreamingActive() const;

    bool sendSysExToHardware (const std::vector<uint8_t>& sysexData);
    bool sendMidiCCToHardware (uint8_t channel, uint8_t ccNum, uint8_t value);
    bool sendMidiMessageToHardware (const juce::MidiMessage& msg);
    bool queueMidiMessageToHardware (const juce::MidiMessage& msg) noexcept;
    bool requestPatchMemoryFromHardware();
    void reinitializeHardwareConnection();

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    void syncAllParametersToHardware();

    std::atomic<int> lastRxCC { -1 };
    std::atomic<int> lastRxVal { -1 };
    std::atomic<bool> hasPerformedInitialHardwareSync { false };
    std::atomic<bool> isSyncingFromHardware { false };

    void setLastRxParamName (const juce::String& name)
    {
        std::lock_guard<std::mutex> lock (rxMutex);
        lastRxParamName = name;
    }

    juce::String getLastRxParamName()
    {
        std::lock_guard<std::mutex> lock (rxMutex);
        return lastRxParamName;
    }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static uint8_t getToneParamOffset (const juce::String& baseParamId);
    static juce::String getBaseParamIdByToneOffset (uint8_t offset);
    static juce::String getPatchCommonParamIdByOffset (uint8_t offset);
    void sendToneSelectToHardware (int part); // 0 = Upper, 1 = Lower

    bool isStereoModeActive();
    bool isStereoExcludeOscControls();
    void activateStereoMode();
    void deactivateStereoMode();
    void applyStereoWidth();
    void handleStereoModeParameterChange (const juce::String& paramId, float newValue);
    void handleLinkModeParameterChange (const juce::String& paramId, float newValue);

    int getCurrentTonePage() const { return currentTonePage.load(); }
    void setCurrentTonePage (int page) { currentTonePage.store (page); }

    // Phase 3: Allow patch serializer to suppress SysEx echo during bulk patch loads
    void setSuppressHardwareEcho (bool suppress) { isUpdatingFromHardware.store (suppress); }
    void markParameterUpdatedFromHardware (const juce::String& paramId);

    void timerCallback() override;

private:
    std::mutex rxMutex;
    std::mutex rxTimeMutex;
    std::mutex txTimeMutex;
    std::unordered_map<juce::String, uint32_t> lastHardwareRxTime;
    std::unordered_map<juce::String, uint32_t> lastHardwareTxTime;
    std::unordered_map<juce::String, float> pendingHardwareUpdates;
    std::unordered_map<juce::String, float> pendingMirroredHardwareUpdates;
    std::atomic<uint32_t> lastHardwareKnobActivityTime { 0 };
    juce::String lastRxParamName { "None" };
    void setupParameterListeners();
    void sendParameterChangeToHardware (const juce::String& paramId, float value);
    void handleIncomingSysEx (const std::vector<uint8_t>& sysexData);
    void handleIncomingMidiCC (uint8_t channel, uint8_t ccNum, uint8_t value);
    void syncHostTempoAndClock (juce::AudioPlayHead* playHead, int numSamples);

    double lastHostBpm = -1.0;
    int64_t lastClockPulse = -1;
    bool wasPlaying = false;

    std::atomic<bool> isUpdatingFromHardware { false };
    std::atomic<bool> isUpdatingStereoMirror { false };
    std::atomic<bool> hasLoadedDAWProjectState { false };
    std::atomic<int> currentTonePage { 0 }; // 0 = Upper, 1 = Lower
    std::atomic<int> cachedEditorMode { 0 }; // Cached editor mode: 0 = USB MIDI+Audio, 1 = USB MIDI Only, 2 = DIN MIDI
    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<SH201USBManager> usbManager;
    std::unique_ptr<SH201DinMidiManager> dinMidiManager;

    // We will attach an APVTS parameter change callback listener
    class ParameterListener : public juce::AudioProcessorValueTreeState::Listener
    {
    public:
        ParameterListener (SH201AudioProcessor& p) : processor (p) {}
        void parameterChanged (const juce::String& parameterID, float newValue) override
        {
            processor.sendParameterChangeToHardware (parameterID, newValue);
            processor.handleStereoModeParameterChange (parameterID, newValue);
            processor.handleLinkModeParameterChange (parameterID, newValue);
        }
    private:
        SH201AudioProcessor& processor;
    };

    std::unique_ptr<ParameterListener> paramListener;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SH201AudioProcessor)
};
