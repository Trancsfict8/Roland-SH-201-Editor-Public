#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Patches/SH201PatchSerializer.h"
#include "Utils/GlobalSettings.h"

SH201AudioProcessor::SH201AudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SH201_PARAMETERS", createParameterLayout())
{
    // Initialize the state value tree explicitly so all parameters are tracked and saved
    apvts.state = juce::ValueTree ("SH201_PARAMETERS");

    usbManager = std::make_unique<SH201USBManager>();
    dinMidiManager = std::make_unique<SH201DinMidiManager>();
    paramListener = std::make_unique<ParameterListener> (*this);

    setupParameterListeners();

    auto onSysEx = [this] (const std::vector<uint8_t>& sysexData)
    {
        handleIncomingSysEx (sysexData);
    };

    auto onCC = [this] (uint8_t ch, uint8_t ccNum, uint8_t val)
    {
        handleIncomingMidiCC (ch, ccNum, val);
    };

    auto onPC = [this] (uint8_t ch, uint8_t program)
    {
        juce::ignoreUnused (ch, program);
        juce::MessageManager::callAsync ([this]() {
            if (isHardwareConnected())
            {
                juce::Logger::writeToLog ("SH201AudioProcessor: Hardware Program Change received. Pulling patch from synth to sync UI...");
                
                // Allow the immediate automatic dump from the hardware to be parsed
                isSyncingFromHardware.store (true);
                
                // Delay the manual patch memory request to ensure it doesn't collide with the automatic dump,
                // and to guarantee we fetch the lower tone (which may be omitted in the auto dump).
                juce::Timer::callAfterDelay (350, [this]() {
                    isSyncingFromHardware.store (true);
                    requestPatchMemoryFromHardware();
                    
                    // Safety timeout: ensure the flag clears even if the hardware drops a SysEx response
                    juce::Timer::callAfterDelay (500, [this]() {
                        isSyncingFromHardware.store (false);
                    });
                });
            }
        });
    };

    // Hook up Phase 3 bi-directional SysEx receiver and MIDI listeners for both USB and DIN MIDI
    usbManager->onSysExReceived = onSysEx;
    usbManager->onMidiCCReceived = onCC;
    usbManager->onProgramChangeReceived = onPC;

    dinMidiManager->onSysExReceived = onSysEx;
    dinMidiManager->onMidiCCReceived = onCC;
    dinMidiManager->onProgramChangeReceived = onPC;

    reinitializeHardwareConnection();

    // Schedule an initial patch pull from hardware if the DAW hasn't loaded a project
    juce::Timer::callAfterDelay (500, [this]() {
        if (! hasLoadedDAWProjectState.load() && ! hasPerformedInitialHardwareSync.load())
        {
            juce::Logger::writeToLog ("SH201AudioProcessor: No DAW project state found on startup. Syncing from hardware...");
            hasPerformedInitialHardwareSync.store (true);
            isSyncingFromHardware.store (true);
            if (isHardwareConnected())
            {
                requestPatchMemoryFromHardware();
                // DIN MIDI is much slower than USB (31.25 kbaud), so allow more time for the full patch dump
                int syncTimeout = (cachedEditorMode.load() == 2) ? 2000 : 500;
                juce::Timer::callAfterDelay (syncTimeout, [this]() {
                    isSyncingFromHardware.store (false);
                });
            }
        }
    });

    startTimerHz (60);
}

SH201AudioProcessor::~SH201AudioProcessor()
{
    stopTimer();
    for (auto* param : getParameters())
    {
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
            apvts.removeParameterListener (p->paramID, paramListener.get());
    }

    if (dinMidiManager)
        dinMidiManager->closeDevices();

    if (usbManager)
        usbManager->shutdown();
}

bool SH201AudioProcessor::isHardwareConnected() const
{
    int mode = cachedEditorMode.load();
    if (mode == 2)
        return dinMidiManager && dinMidiManager->isConnected();
    return usbManager && usbManager->isConnected();
}

juce::String SH201AudioProcessor::getHardwareConnectionStatus() const
{
    int mode = cachedEditorMode.load();
    if (mode == 2)
        return dinMidiManager ? dinMidiManager->getConnectionStatus() : "DIN MIDI Disconnected";

    if (usbManager && usbManager->isConnected())
        return "USB CONNECTED: " + usbManager->getConnectionMode();
    return "USB DISCONNECTED - Hardware Not Found";
}

bool SH201AudioProcessor::isAudioStreamingActive() const
{
    int mode = cachedEditorMode.load();
    if (mode == 2)
        return false;
    return usbManager && usbManager->isAudioStreaming();
}

bool SH201AudioProcessor::sendSysExToHardware (const std::vector<uint8_t>& sysexData)
{
    int mode = cachedEditorMode.load();
    if (mode == 2)
        return dinMidiManager && dinMidiManager->sendSysEx (sysexData);
    return usbManager && usbManager->sendSysEx (sysexData);
}

bool SH201AudioProcessor::sendMidiCCToHardware (uint8_t channel, uint8_t ccNum, uint8_t value)
{
    int mode = cachedEditorMode.load();
    if (mode == 2)
        return dinMidiManager && dinMidiManager->sendMidiCC (channel, ccNum, value);
    return usbManager && usbManager->sendMidiCC (channel, ccNum, value);
}

bool SH201AudioProcessor::sendMidiMessageToHardware (const juce::MidiMessage& msg)
{
    int mode = cachedEditorMode.load();
    if (mode == 2)
        return dinMidiManager && dinMidiManager->sendMidiMessage (msg);
    return usbManager && usbManager->sendMidiMessage (msg);
}

bool SH201AudioProcessor::queueMidiMessageToHardware (const juce::MidiMessage& msg) noexcept
{
    int mode = cachedEditorMode.load();
    if (mode == 2)
        return dinMidiManager && dinMidiManager->queueMidiMessage (msg);
    return usbManager && usbManager->queueMidiMessageForUsb (msg);
}

bool SH201AudioProcessor::requestPatchMemoryFromHardware()
{
    int mode = cachedEditorMode.load();
    if (mode == 2)
        return dinMidiManager && dinMidiManager->requestPatchMemory();
    return usbManager && usbManager->requestPatchMemory();
}

void SH201AudioProcessor::reinitializeHardwareConnection()
{
    int mode = GlobalSettings::getEditorMode();
    cachedEditorMode.store (mode);

    if (mode == 2)
    {
        if (usbManager)
            usbManager->shutdown();

        if (dinMidiManager)
        {
            auto inId = GlobalSettings::getDinMidiIn();
            auto inName = GlobalSettings::getDinMidiInName();
            auto outId = GlobalSettings::getDinMidiOut();
            auto outName = GlobalSettings::getDinMidiOutName();
            dinMidiManager->openDevices (inId, inName, outId, outName);
        }
    }
    else
    {
        if (dinMidiManager)
            dinMidiManager->closeDevices();

        if (usbManager)
            usbManager->initialize();
    }
}

static const std::vector<juce::String>& getBaseToneParamIds()
{
    static const std::vector<juce::String> ids = {
        "osc1_wave", "osc1_pitch_wide", "osc1_pitch", "osc1_fine_tune", "osc1_pulse_width", "osc1_pitch_env_depth",
        "osc2_wave", "osc2_pitch_wide", "osc2_pitch", "osc2_detune", "osc2_pulse_width", "osc2_pitch_env_depth",
        "pitch_env_attack", "pitch_env_decay",
        "mix_mod_type", "mix_mod_balance", "mix_mod_low_freq",
        "filter_type", "filter_slope", "filter_cutoff", "filter_keyfollow", "filter_velocity_sens", "filter_resonance",
        "filter_attack", "filter_decay", "filter_sustain", "filter_release", "filter_env_depth",
        "amp_overdrive_switch", "overdrive_level", "amp_level", "amp_velocity_sens", "amp_pan",
        "amp_attack", "amp_decay", "amp_sustain", "amp_release",
        "lfo1_shape", "lfo1_rate", "lfo1_tempo_sync", "lfo1_tempo_note", "lfo1_fade_time", "lfo1_key_trigger", "lfo1_dest1", "lfo1_depth", "lfo1_dest2", "lfo1_depth2",
        "lfo2_shape", "lfo2_rate", "lfo2_tempo_sync", "lfo2_tempo_note", "lfo2_fade_time", "lfo2_key_trigger", "lfo2_dest1", "lfo2_depth", "lfo2_dest2", "lfo2_depth2"
    };
    return ids;
}

juce::AudioProcessorValueTreeState::ParameterLayout SH201AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto addIntParam = [&layout] (const juce::String& id, const juce::String& name, int defVal)
    {
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID (id, 1), name, 0, 127, defVal));
    };

    auto addSignedParam = [&layout] (const juce::String& id, const juce::String& name, int minVal, int maxVal, int defVal)
    {
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID (id, 1), name, minVal, maxVal, defVal));
    };

    auto addChoiceParam = [&layout] (const juce::String& id, const juce::String& name, const juce::StringArray& choices, int defVal)
    {
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (id, 1), name, choices, defVal));
    };

    auto addRangedIntParam = [&layout] (const juce::String& id, const juce::String& name, int minVal, int maxVal, int defVal)
    {
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID (id, 1), name, minVal, maxVal, defVal));
    };

    // Helper to add all 48 Tone Parameters for either Upper Tone or Lower Tone
    auto addToneParameters = [&] (const juce::String& prefix, const juce::String& dispPrefix)
    {
        // --- Oscillator 1 ---
        addChoiceParam (prefix + "osc1_wave", dispPrefix + "Osc 1 Waveform", { "Saw", "Square", "PW-Square", "Triangle", "Sine", "Noise", "Feedback", "Supersaw", "External" }, 0);
        addChoiceParam (prefix + "osc1_pitch_wide", dispPrefix + "Osc 1 Pitch Wide", { "OFF", "ON" }, 0);
        addSignedParam (prefix + "osc1_pitch", dispPrefix + "Osc 1 Coarse Tune", -36, 36, 0); // Corrected PDF range (-36 to +36 semitones)
        addSignedParam (prefix + "osc1_fine_tune", dispPrefix + "Osc 1 Fine Tune", -50, 50, 0);
        addIntParam    (prefix + "osc1_pulse_width", dispPrefix + "Osc 1 Pulse Width", 64);
        addSignedParam (prefix + "osc1_pitch_env_depth", dispPrefix + "Osc 1 Pitch Env Depth", -63, 63, 0);

        // --- Oscillator 2 ---
        addChoiceParam (prefix + "osc2_wave", dispPrefix + "Osc 2 Waveform", { "Saw", "Square", "PW-Square", "Triangle", "Sine", "Noise", "Feedback", "Supersaw", "External" }, 0);
        addChoiceParam (prefix + "osc2_pitch_wide", dispPrefix + "Osc 2 Pitch Wide", { "OFF", "ON" }, 0);
        addSignedParam (prefix + "osc2_pitch", dispPrefix + "Osc 2 Coarse Tune", -36, 36, 0); // Corrected PDF range (-36 to +36 semitones)
        addSignedParam (prefix + "osc2_detune", dispPrefix + "Osc 2 Fine Tune", -50, 50, 0);
        addIntParam    (prefix + "osc2_pulse_width", dispPrefix + "Osc 2 Pulse Width", 64);
        addSignedParam (prefix + "osc2_pitch_env_depth", dispPrefix + "Osc 2 Pitch Env Depth", -63, 63, 0);

        // --- Pitch Envelope ---
        addIntParam    (prefix + "pitch_env_attack", dispPrefix + "Pitch Env Attack", 0);
        addIntParam    (prefix + "pitch_env_decay", dispPrefix + "Pitch Env Decay", 64);

        // --- Mix / Mod ---
        addChoiceParam (prefix + "mix_mod_type", dispPrefix + "Mix/Mod Type", { "MIX", "SYNC", "RING" }, 0);
        addSignedParam (prefix + "mix_mod_balance", dispPrefix + "Mix/Mod Balance", -63, 63, 0);
        addChoiceParam (prefix + "mix_mod_low_freq", dispPrefix + "Mix/Mod Low Freq", { "FLAT", "BOOST", "CUT" }, 0);

        // --- Filter ---
        addChoiceParam (prefix + "filter_type", dispPrefix + "Filter Type", { "BYPASS", "LPF", "HPF", "BPF" }, 1);
        addChoiceParam (prefix + "filter_slope", dispPrefix + "Filter Slope", { "-12 dB", "-24 dB" }, 1);
        addIntParam    (prefix + "filter_cutoff", dispPrefix + "Filter Cutoff", 64);
        addSignedParam (prefix + "filter_keyfollow", dispPrefix + "Filter Key Follow", -200, 200, 0);
        addSignedParam (prefix + "filter_velocity_sens", dispPrefix + "Filter Velocity Sens", -63, 63, 0);
        addIntParam    (prefix + "filter_resonance", dispPrefix + "Filter Resonance", 32);

        // --- Filter Envelope ---
        addIntParam    (prefix + "filter_attack", dispPrefix + "Filter Attack", 0);
        addIntParam    (prefix + "filter_decay", dispPrefix + "Filter Decay", 64);
        addIntParam    (prefix + "filter_sustain", dispPrefix + "Filter Sustain", 64);
        addIntParam    (prefix + "filter_release", dispPrefix + "Filter Release", 40);
        addSignedParam (prefix + "filter_env_depth", dispPrefix + "Filter Env Depth", -63, 63, 32);

        // --- Amp & Overdrive ---
        addChoiceParam (prefix + "amp_overdrive_switch", dispPrefix + "Overdrive Switch", { "OFF", "ON" }, 0);
        addIntParam    (prefix + "overdrive_level", dispPrefix + "Overdrive Drive", 0);
        addIntParam    (prefix + "amp_level", dispPrefix + "Amp Level", 100);
        addSignedParam (prefix + "amp_velocity_sens", dispPrefix + "Amp Velocity Sens", -63, 63, 0);
        addSignedParam (prefix + "amp_pan", dispPrefix + "Amp Pan", -64, 63, 0);

        // --- Amp Envelope ---
        addIntParam    (prefix + "amp_attack", dispPrefix + "Amp Attack", 0);
        addIntParam    (prefix + "amp_decay", dispPrefix + "Amp Decay", 64);
        addIntParam    (prefix + "amp_sustain", dispPrefix + "Amp Sustain", 127);
        addIntParam    (prefix + "amp_release", dispPrefix + "Amp Release", 40);

        // --- LFO 1 ---
        addChoiceParam (prefix + "lfo1_shape", dispPrefix + "LFO 1 Shape", { "TRI", "SIN", "SAW", "SQR", "TRP", "S&H", "RND" }, 0);
        addIntParam    (prefix + "lfo1_rate", dispPrefix + "LFO 1 Rate", 50);
        addChoiceParam (prefix + "lfo1_tempo_sync", dispPrefix + "LFO 1 Tempo Sync", { "ON", "OFF" }, 1); // Corrected PDF order (0=ON, 1=OFF)
        addChoiceParam (prefix + "lfo1_tempo_note", dispPrefix + "LFO 1 Sync Note", { "16", "12", "8", "4", "2", "1", "3/4", "2/3", "1/2", "3/8", "1/3", "1/4", "3/16", "1/6", "1/8", "3/32", "1/12", "1/16", "1/24", "1/32" }, 5);
        addIntParam    (prefix + "lfo1_fade_time", dispPrefix + "LFO 1 Fade Time", 0);
        addChoiceParam (prefix + "lfo1_key_trigger", dispPrefix + "LFO 1 Key Trigger", { "OFF", "ON" }, 1);
        addChoiceParam (prefix + "lfo1_dest1", dispPrefix + "LFO 1 Dest 1", { "PITCH1", "PW1", "FILTER", "AUDIO-FILT" }, 2);
        addSignedParam (prefix + "lfo1_depth", dispPrefix + "LFO 1 Depth 1", -63, 63, 0);
        addChoiceParam (prefix + "lfo1_dest2", dispPrefix + "LFO 1 Dest 2", { "PITCH2", "PW2", "AMP" }, 0);
        addSignedParam (prefix + "lfo1_depth2", dispPrefix + "LFO 1 Depth 2", -63, 63, 0);

        // --- LFO 2 ---
        addChoiceParam (prefix + "lfo2_shape", dispPrefix + "LFO 2 Shape", { "TRI", "SIN", "SAW", "SQR", "TRP", "S&H", "RND" }, 0);
        addIntParam    (prefix + "lfo2_rate", dispPrefix + "LFO 2 Rate", 50);
        addChoiceParam (prefix + "lfo2_tempo_sync", dispPrefix + "LFO 2 Tempo Sync", { "ON", "OFF" }, 1); // Corrected PDF order (0=ON, 1=OFF)
        addChoiceParam (prefix + "lfo2_tempo_note", dispPrefix + "LFO 2 Sync Note", { "16", "12", "8", "4", "2", "1", "3/4", "2/3", "1/2", "3/8", "1/3", "1/4", "3/16", "1/6", "1/8", "3/32", "1/12", "1/16", "1/24", "1/32" }, 5);
        addIntParam    (prefix + "lfo2_fade_time", dispPrefix + "LFO 2 Fade Time", 0);
        addChoiceParam (prefix + "lfo2_key_trigger", dispPrefix + "LFO 2 Key Trigger", { "OFF", "ON" }, 1);
        addChoiceParam (prefix + "lfo2_dest1", dispPrefix + "LFO 2 Dest 1", { "PITCH1", "PW1", "FILTER", "AUDIO-FILT" }, 2);
        addSignedParam (prefix + "lfo2_depth", dispPrefix + "LFO 2 Depth 1", -63, 63, 0);
        addChoiceParam (prefix + "lfo2_dest2", dispPrefix + "LFO 2 Dest 2", { "PITCH2", "PW2", "AMP" }, 0);
        addSignedParam (prefix + "lfo2_depth2", dispPrefix + "LFO 2 Depth 2", -63, 63, 0);
    };

    // Add Upper Tone and Lower Tone parameters
    addToneParameters ("upper_", "Upper ");
    addToneParameters ("lower_", "Lower ");

    // --- Common / Master Parameters (Never change on Page Switch) ---
    addSignedParam ("common_tone_balance", "Tone Balance", -63, 63, 0);
    addChoiceParam ("common_keyboard_mode", "Keyboard Mode", { "SINGLE", "DUAL", "SPLIT" }, 0);
    addIntParam    ("common_split_point", "Split Point", 60);
    addChoiceParam ("common_delay_switch", "Delay Switch", { "OFF", "ON" }, 1);
    addChoiceParam ("common_reverb_switch", "Reverb Switch", { "OFF", "ON" }, 1);
    addIntParam    ("common_delay_level", "Delay Level", 0);
    addIntParam    ("common_reverb_level", "Reverb Level", 30);
    addSignedParam ("common_pitch_bend_range", "Pitch Bend Range", 0, 24, 2);
    addSignedParam ("common_octave_shift", "Octave Shift", -3, 3, 0);
    addChoiceParam ("common_portamento_switch", "Portamento Switch", { "OFF", "ON" }, 0);
    addIntParam    ("common_portamento_time", "Portamento Time", 0);
    addChoiceParam ("common_mono_solo_select", "Mono/Solo Select", { "POLY", "SOLO+L", "SOLO" }, 0);

    // --- Stereo Mode / Roland SH-201 Editor Features ---
    addChoiceParam ("common_stereo_mode", "Stereo Mode", { "OFF", "ON" }, 0);
    addSignedParam ("common_stereo_width", "Stereo Width", 0, 100, 100);
    addChoiceParam ("common_stereo_link_mode", "Param Link Mode", { "LINK ALL", "EXCLUDE OSC" }, 0);

    // --- Panel Links ---
    addChoiceParam ("link_osc1", "Link OSC 1", { "OFF", "ON" }, 0);
    addChoiceParam ("link_osc2", "Link OSC 2", { "OFF", "ON" }, 0);
    addChoiceParam ("link_mix_mod", "Link Mix/Mod", { "OFF", "ON" }, 0);
    addChoiceParam ("link_pitch_env", "Link Pitch Env", { "OFF", "ON" }, 0);
    addChoiceParam ("link_filter", "Link Filter", { "OFF", "ON" }, 0);
    addChoiceParam ("link_filter_env", "Link Filter Env", { "OFF", "ON" }, 0);
    addChoiceParam ("link_amp_env", "Link Amp Env", { "OFF", "ON" }, 0);
    addChoiceParam ("link_amp", "Link Amp", { "OFF", "ON" }, 0);
    addChoiceParam ("link_lfo1", "Link LFO 1", { "OFF", "ON" }, 0);
    addChoiceParam ("link_lfo2", "Link LFO 2", { "OFF", "ON" }, 0);

    // --- Actual Patch Common ---
    addRangedIntParam ("common_patch_level", "Patch Level", 0, 127, 127);
    addRangedIntParam ("common_patch_tempo", "Patch Tempo", 5, 300, 120);
    addChoiceParam    ("common_keyboard_part", "Keyboard Part", { "UPPER", "LOWER" }, 0);
    addChoiceParam    ("common_split_arpeggio", "Split Arp", { "UPPER", "LOWER", "BOTH" }, 0);
    addChoiceParam    ("common_mod_destination", "Mod Dest", { "UPPER", "LOWER", "BOTH" }, 2);
    addChoiceParam    ("common_dbeam_destination", "D Beam Dest", { "UPPER", "LOWER", "BOTH" }, 2);
    addChoiceParam    ("common_pitch_bend_destination", "Pitch Bend Dest", { "UPPER", "LOWER", "BOTH" }, 2);
    addChoiceParam    ("common_expression_destination", "Expression Dest", { "UPPER", "LOWER", "BOTH" }, 2);
    addChoiceParam    ("common_active_expression_switch", "Active Expr Sw", { "OFF", "ON" }, 1);
    addChoiceParam    ("common_arpeggio_switch", "Arpeggio Sw", { "OFF", "ON" }, 0);
    addChoiceParam    ("common_arpeggio_hold", "Arpeggio Hold", { "OFF", "ON" }, 0);
    addChoiceParam    ("common_modulation_assign", "Mod Assign", { "OSC1&2", "OSC1", "OSC2", "PW1", "PW2", "FILTER", "AMP", "AUD-FILT" }, 0);
    addChoiceParam    ("common_dbeam_assign", "D Beam Assign", { "O1-PITCH", "O1-DETUNE", "O1-PW", "O2-PITCH", "O2-DETUNE", "O2-PW", "MIX-BAL", "FILT-CUT", "FILT-RES", "FILT-KEYFOL", "AMP-LEVEL", "AUD-CUT", "AUD-RES", "P-ENV-A", "P-ENV-D", "O1-P-DEP", "O2-P-DEP", "LFO1-RATE", "LFO1-DEP1", "LFO1-DEP2", "LFO2-RATE", "LFO2-DEP1", "LFO2-DEP2", "F-ENV-A", "F-ENV-D", "F-ENV-S", "F-ENV-R", "F-ENV-DEP", "A-ENV-A", "A-ENV-D", "A-ENV-S", "A-ENV-R", "DLY-TIME", "DLY-DEP", "REV-TIME", "REV-DEP", "BENDER" }, 7);
    addChoiceParam    ("common_dbeam_polarity", "D Beam Polarity", { "NORMAL", "REVERSE" }, 0);

    // --- System Settings ---
    addRangedIntParam("patch_bank_msb", "Patch Bank MSB", 0, 127, 0);
    addRangedIntParam("patch_bank_lsb", "Patch Bank LSB", 0, 127, 0);
    addRangedIntParam("patch_program_number", "Patch Program Number", 0, 127, 0);
    addRangedIntParam("patch_rx_tx_channel", "Patch Rx/Tx Channel", 0, 15, 0);
    addChoiceParam ("patch_remain", "Patch Remain", { "OFF", "ON" }, 0);
    addSignedParam ("master_tune", "Master Tune", -100, 100, 0);
    addSignedParam ("master_key_shift", "Master Key Shift", -24, 24, 0);
    addRangedIntParam("master_level", "Master Level", 0, 127, 127);
    addChoiceParam ("system_clock_source", "System Clock Source", { "PATCH", "SYSTEM", "MIDI", "USB" }, 0);
    addRangedIntParam("system_tempo", "System Tempo", 5, 300, 120);
    addChoiceParam ("receive_program_change", "Receive Program Change", { "OFF", "ON" }, 1);
    addChoiceParam ("receive_bank_select", "Receive Bank Select", { "OFF", "ON" }, 1);
    addChoiceParam ("transmit_edit_data", "Transmit Edit Data", { "OFF", "ON" }, 1);
    addChoiceParam ("soft_through", "Soft Through", { "OFF", "ON" }, 0);
    addChoiceParam ("midi_usb_thru", "MIDI-USB Thru", { "OFF", "ON" }, 0);
    addChoiceParam ("transmit_program_change", "Transmit Program Change", { "OFF", "ON" }, 1);
    addChoiceParam ("transmit_bank_select", "Transmit Bank Select", { "OFF", "ON" }, 1);
    addChoiceParam ("transmit_active_sensing", "Transmit Active Sensing", { "OFF", "ON" }, 1);
    addSignedParam ("transpose_value", "Transpose Value", -5, 6, 0);
    addChoiceParam ("local_switch", "Local Switch", { "OFF", "ON" }, 1);
    addChoiceParam ("remote_keyboard_switch", "Remote Keyboard Switch", { "OFF", "ON" }, 0);
    addChoiceParam ("pedal_polarity", "Pedal Polarity", { "STANDARD", "REVERSE" }, 0);
    addChoiceParam ("pedal_assign", "Pedal Assign", { "HOLD", "MODULATION", "VOLUME", "BALANCE", "PAN", "EXPRESSION", "START/STOP", "ARP-SW" }, 0);
    addRangedIntParam("d_beam_sens", "D Beam Sens", 1, 8, 4);
    addChoiceParam ("recorder_sync_output", "Recorder Sync Output", { "OFF", "ON" }, 0);
    addChoiceParam ("recorder_metronome_mode", "Recorder Metronome Mode", { "OFF", "REC-ONLY", "REC&PLAY", "ALWAYS" }, 0);
    addRangedIntParam("recorder_metronome_level", "Recorder Metronome Level", 0, 7, 4);

    // --- Patch Delay ---
    addRangedIntParam("patch_delay_time", "Delay Time", 0, 127, 64);
    addRangedIntParam("patch_delay_feedback", "Delay Feedback", 0, 98, 49);
    addChoiceParam("patch_delay_hf_damp", "Delay HF Damp", { "200", "250", "315", "400", "500", "630", "800", "1000", "1250", "1600", "2000", "2500", "3150", "4000", "5000", "6300", "8000", "BYPASS" }, 17);
    addRangedIntParam("patch_delay_mod_rate", "Delay Mod Rate", 0, 127, 0);
    addRangedIntParam("patch_delay_mod_depth", "Delay Mod Depth", 0, 127, 0);

    // --- Patch Reverb ---
    addRangedIntParam("patch_reverb_time", "Reverb Time", 0, 127, 64);
    addRangedIntParam("patch_reverb_pre_delay", "Reverb Pre Delay", 0, 125, 0);
    addRangedIntParam("patch_reverb_size", "Reverb Size", 0, 7, 4);
    addChoiceParam("patch_reverb_high_cut", "Reverb High Cut", { "160", "200", "250", "320", "400", "500", "640", "800", "1000", "1250", "1600", "2000", "2500", "3200", "4000", "5000", "6400", "8000", "10000", "12500", "BYPASS" }, 20);
    addRangedIntParam("patch_reverb_density", "Reverb Density", 0, 127, 127);
    addRangedIntParam("patch_reverb_diffusion", "Reverb Diffusion", 0, 127, 127);
    addChoiceParam("patch_reverb_lf_damp_freq", "Reverb LF Damp Freq", { "50", "64", "80", "100", "125", "160", "200", "250", "320", "400", "500", "640", "800", "1000", "1250", "1600", "2000", "2500", "3200", "4000" }, 9);
    addRangedIntParam("patch_reverb_lf_damp_gain", "Reverb LF Damp Gain", 0, 36, 36);
    addChoiceParam("patch_reverb_hf_damp_freq", "Reverb HF Damp Freq", { "4000", "5000", "6400", "8000", "10000", "12500" }, 5);
    addRangedIntParam("patch_reverb_hf_damp_gain", "Reverb HF Damp Gain", 0, 36, 36);


    return layout;
}

void SH201AudioProcessor::setupParameterListeners()
{
    for (auto* param : getParameters())
    {
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
            apvts.addParameterListener (p->paramID, paramListener.get());
    }
}

uint8_t SH201AudioProcessor::getToneParamOffset (const juce::String& baseParamId)
{
    static const std::map<juce::String, uint8_t> offsetMap = {
        { "osc1_wave", 0x00 }, { "osc1_pitch_wide", 0x01 }, { "osc1_pitch", 0x02 }, { "osc1_fine_tune", 0x03 }, { "osc1_pulse_width", 0x04 }, { "osc1_pitch_env_depth", 0x05 },
        { "osc2_wave", 0x06 }, { "osc2_pitch_wide", 0x07 }, { "osc2_pitch", 0x08 }, { "osc2_detune", 0x09 }, { "osc2_pulse_width", 0x0A }, { "osc2_pitch_env_depth", 0x0B },
        { "pitch_env_attack", 0x0C }, { "pitch_env_decay", 0x0D },
        { "mix_mod_type", 0x0E }, { "mix_mod_balance", 0x0F }, { "mix_mod_low_freq", 0x10 },
        { "filter_type", 0x11 }, { "filter_slope", 0x12 }, { "filter_cutoff", 0x13 }, { "filter_keyfollow", 0x14 }, { "filter_velocity_sens", 0x15 }, { "filter_resonance", 0x16 },
        { "filter_attack", 0x17 }, { "filter_decay", 0x18 }, { "filter_sustain", 0x19 }, { "filter_release", 0x1A }, { "filter_env_depth", 0x1B },
        { "amp_overdrive_switch", 0x1C }, { "overdrive_level", 0x1D }, { "amp_level", 0x1E }, { "amp_velocity_sens", 0x1F }, { "amp_pan", 0x20 },
        { "amp_attack", 0x21 }, { "amp_decay", 0x22 }, { "amp_sustain", 0x23 }, { "amp_release", 0x24 },
        { "lfo1_shape", 0x27 }, { "lfo1_rate", 0x28 }, { "lfo1_tempo_sync", 0x29 }, { "lfo1_tempo_note", 0x2A }, { "lfo1_fade_time", 0x2B }, { "lfo1_key_trigger", 0x2C }, { "lfo1_dest1", 0x2D }, { "lfo1_depth", 0x2E }, { "lfo1_dest2", 0x2F }, { "lfo1_depth2", 0x30 },
        { "lfo2_shape", 0x31 }, { "lfo2_rate", 0x32 }, { "lfo2_tempo_sync", 0x33 }, { "lfo2_tempo_note", 0x34 }, { "lfo2_fade_time", 0x35 }, { "lfo2_key_trigger", 0x36 }, { "lfo2_dest1", 0x37 }, { "lfo2_depth", 0x38 }, { "lfo2_dest2", 0x39 }, { "lfo2_depth2", 0x3A }
    };
    auto it = offsetMap.find (baseParamId);
    return it != offsetMap.end() ? it->second : 0xFF;
}

juce::String SH201AudioProcessor::getBaseParamIdByToneOffset (uint8_t offset)
{
    static const std::map<uint8_t, juce::String> offsetToIdMap = {
        { 0x00, "osc1_wave" }, { 0x01, "osc1_pitch_wide" }, { 0x02, "osc1_pitch" }, { 0x03, "osc1_fine_tune" }, { 0x04, "osc1_pulse_width" }, { 0x05, "osc1_pitch_env_depth" },
        { 0x06, "osc2_wave" }, { 0x07, "osc2_pitch_wide" }, { 0x08, "osc2_pitch" }, { 0x09, "osc2_detune" }, { 0x0A, "osc2_pulse_width" }, { 0x0B, "osc2_pitch_env_depth" },
        { 0x0C, "pitch_env_attack" }, { 0x0D, "pitch_env_decay" },
        { 0x0E, "mix_mod_type" }, { 0x0F, "mix_mod_balance" }, { 0x10, "mix_mod_low_freq" },
        { 0x11, "filter_type" }, { 0x12, "filter_slope" }, { 0x13, "filter_cutoff" }, { 0x14, "filter_keyfollow" }, { 0x15, "filter_velocity_sens" }, { 0x16, "filter_resonance" },
        { 0x17, "filter_attack" }, { 0x18, "filter_decay" }, { 0x19, "filter_sustain" }, { 0x1A, "filter_release" }, { 0x1B, "filter_env_depth" },
        { 0x1C, "amp_overdrive_switch" }, { 0x1D, "overdrive_level" }, { 0x1E, "amp_level" }, { 0x1F, "amp_velocity_sens" }, { 0x20, "amp_pan" },
        { 0x21, "amp_attack" }, { 0x22, "amp_decay" }, { 0x23, "amp_sustain" }, { 0x24, "amp_release" },
        { 0x27, "lfo1_shape" }, { 0x28, "lfo1_rate" }, { 0x29, "lfo1_tempo_sync" }, { 0x2A, "lfo1_tempo_note" }, { 0x2B, "lfo1_fade_time" }, { 0x2C, "lfo1_key_trigger" }, { 0x2D, "lfo1_dest1" }, { 0x2E, "lfo1_depth" }, { 0x2F, "lfo1_dest2" }, { 0x30, "lfo1_depth2" },
        { 0x31, "lfo2_shape" }, { 0x32, "lfo2_rate" }, { 0x33, "lfo2_tempo_sync" }, { 0x34, "lfo2_tempo_note" }, { 0x35, "lfo2_fade_time" }, { 0x36, "lfo2_key_trigger" }, { 0x37, "lfo2_dest1" }, { 0x38, "lfo2_depth" }, { 0x39, "lfo2_dest2" }, { 0x3A, "lfo2_depth2" }
    };
    auto it = offsetToIdMap.find (offset);
    return it != offsetToIdMap.end() ? it->second : juce::String();
}

juce::String SH201AudioProcessor::getPatchCommonParamIdByOffset (uint8_t offset)
{
    static const std::map<uint8_t, juce::String> map = {
        { 0x0C, "common_patch_level" },
        { 0x0D, "common_tone_balance" },
        { 0x11, "common_keyboard_mode" },
        { 0x12, "common_keyboard_part" },
        { 0x13, "common_split_point" },
        { 0x14, "common_split_arpeggio" },
        { 0x15, "common_mod_destination" },
        { 0x16, "common_dbeam_destination" },
        { 0x17, "common_pitch_bend_destination" },
        { 0x18, "common_expression_destination" },
        { 0x19, "common_active_expression_switch" },
        { 0x1A, "common_arpeggio_switch" },
        { 0x1B, "common_arpeggio_hold" },
        { 0x1C, "common_delay_switch" },
        { 0x1D, "common_reverb_switch" },
        { 0x1E, "common_modulation_assign" },
        { 0x1F, "common_dbeam_assign" },
        { 0x20, "common_dbeam_polarity" }
    };
    auto it = map.find (offset);
    return it != map.end() ? it->second : juce::String();
}

void SH201AudioProcessor::markParameterUpdatedFromHardware (const juce::String& paramId)
{
    std::lock_guard<std::mutex> lock (rxTimeMutex);
    lastHardwareRxTime[paramId] = juce::Time::getMillisecondCounter();
}

void SH201AudioProcessor::timerCallback()
{
    // 1. Process pending debounced mirrored updates if the user has paused / finished turning the physical knob
    if (! isUpdatingFromHardware.load())
    {
        uint32_t now = juce::Time::getMillisecondCounter();
        uint32_t lastKnobActivity = lastHardwareKnobActivityTime.load();

        if (lastKnobActivity > 0 && (now - lastKnobActivity) >= 120)
        {
            std::vector<std::pair<juce::String, float>> mirroredToSend;
            {
                std::lock_guard<std::mutex> lock (txTimeMutex);
                if (! pendingMirroredHardwareUpdates.empty())
                {
                    for (const auto& item : pendingMirroredHardwareUpdates)
                        mirroredToSend.push_back (item);
                    pendingMirroredHardwareUpdates.clear();
                }
            }

            for (const auto& item : mirroredToSend)
                sendParameterChangeToHardware (item.first, item.second);
        }
    }

    // 2. Process standard throttled GUI updates
    std::vector<std::pair<juce::String, float>> toSend;
    {
        std::lock_guard<std::mutex> lock (txTimeMutex);
        if (pendingHardwareUpdates.empty())
            return;

        uint32_t now = juce::Time::getMillisecondCounter();
        for (auto it = pendingHardwareUpdates.begin(); it != pendingHardwareUpdates.end(); )
        {
            auto txIt = lastHardwareTxTime.find (it->first);
            if (txIt == lastHardwareTxTime.end() || (now - txIt->second) >= 10)
            {
                toSend.push_back ({ it->first, it->second });
                it = pendingHardwareUpdates.erase (it);
            }
            else
            {
                ++it;
            }
        }
    }

    for (const auto& p : toSend)
        sendParameterChangeToHardware (p.first, p.second);
}

void SH201AudioProcessor::sendParameterChangeToHardware (const juce::String& paramId, float value)
{
    if (isUpdatingFromHardware.load())
        return;

    {
        std::lock_guard<std::mutex> lock (rxTimeMutex);
        auto it = lastHardwareRxTime.find (paramId);
        if (it != lastHardwareRxTime.end())
        {
            uint32_t now = juce::Time::getMillisecondCounter();
            if ((now - it->second) < 500)
            {
                // Suppress SysEx echo back to hardware if we received this parameter from hardware within the last 500ms
                return;
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock (txTimeMutex);
        uint32_t now = juce::Time::getMillisecondCounter();
        auto it = lastHardwareTxTime.find (paramId);
        if (it != lastHardwareTxTime.end() && (now - it->second) < 10)
        {
            // Throttle SysEx transmission to max 100 Hz per parameter to prevent saturating SH-201 CPU/DSP in Stereo (Dual) mode!
            pendingHardwareUpdates[paramId] = value;
            return;
        }
        lastHardwareTxTime[paramId] = now;
        pendingHardwareUpdates.erase (paramId);
    }

    if (! isHardwareConnected())
        return;

    int minVal = 0;
    if (auto* param = apvts.getParameter (paramId))
    {
        if (auto* intParam = dynamic_cast<juce::AudioParameterInt*> (param))
            minVal = intParam->getRange().getStart();
    }

    if (paramId.endsWith ("osc1_pitch_wide") || paramId.endsWith ("osc2_pitch_wide"))
    {
        if (value <= 0.5f)
        {
            juce::String pitchId = paramId.substring (0, paramId.length() - 5);
            if (auto* pitchParam = apvts.getParameter (pitchId))
            {
                float currentPitch = pitchParam->convertFrom0to1 (pitchParam->getValue());
                if (currentPitch < -12.0f || currentPitch > 12.0f)
                {
                    float clampedPitch = juce::jlimit (-12.0f, 12.0f, currentPitch);
                    pitchParam->setValueNotifyingHost (pitchParam->convertTo0to1 (clampedPitch));
                }
            }
        }
    }

    uint8_t intVal = 0;
    if (paramId.endsWith ("filter_keyfollow"))
        intVal = static_cast<uint8_t> (juce::jlimit (44, 84, static_cast<int> (64 + std::round (value / 10.0f))));
    else if (minVal < 0)
    {
        int clampedVal = static_cast<int> (std::round (value));
        if (paramId == "common_tone_balance")
        {
            clampedVal = -clampedVal;
        }
        if (paramId.endsWith ("osc1_pitch"))
        {
            auto* wideParam = apvts.getRawParameterValue (paramId.startsWith ("upper_") ? "upper_osc1_pitch_wide" : "lower_osc1_pitch_wide");
            if (wideParam != nullptr && *wideParam <= 0.5f)
                clampedVal = juce::jlimit (-12, 12, clampedVal) * 3;
        }
        else if (paramId.endsWith ("osc2_pitch"))
        {
            auto* wideParam = apvts.getRawParameterValue (paramId.startsWith ("upper_") ? "upper_osc2_pitch_wide" : "lower_osc2_pitch_wide");
            if (wideParam != nullptr && *wideParam <= 0.5f)
                clampedVal = juce::jlimit (-12, 12, clampedVal) * 3;
        }
        intVal = static_cast<uint8_t> (juce::jlimit (0, 127, clampedVal + 64));
    }
    else
        intVal = static_cast<uint8_t> (juce::jlimit (0, 127, static_cast<int> (std::round (value))));

    // Helper to send a DT1 SysEx message to an address (10 00 addr2 addr3)
    auto sendDT1 = [this, intVal] (uint8_t addr2, uint8_t addr3) {
        uint8_t addr0 = 0x10;
        uint8_t addr1 = 0x00;
        int sum = addr0 + addr1 + addr2 + addr3 + intVal;
        uint8_t checksum = static_cast<uint8_t> ((128 - (sum % 128)) % 128);
        std::vector<uint8_t> dt1Packet = {
            0xF0, 0x41, 0x10, 0x00, 0x00, 0x16, 0x12,
            addr0, addr1, addr2, addr3,
            intVal,
            checksum,
            0xF7
        };
        sendSysExToHardware (dt1Packet);
    };

    // Determine target SysEx address
    if (paramId.startsWith ("upper_"))
    {
        uint8_t offset = getToneParamOffset (paramId.substring (6));
        if (offset != 0xFF)
            sendDT1 (0x01, offset);
    }
    else if (paramId.startsWith ("lower_"))
    {
        uint8_t offset = getToneParamOffset (paramId.substring (6));
        if (offset != 0xFF)
            sendDT1 (0x02, offset);
    }
    else if (paramId.startsWith ("common_"))
    {
        if      (paramId == "common_tone_balance")        sendDT1 (0x00, 0x0D);
        else if (paramId == "common_keyboard_mode")       sendDT1 (0x00, 0x11);
        else if (paramId == "common_split_point")         sendDT1 (0x00, 0x13);
        else if (paramId == "common_delay_switch")        sendDT1 (0x00, 0x1C);
        else if (paramId == "common_reverb_switch")       sendDT1 (0x00, 0x1D);
        else if (paramId == "common_delay_level")         { sendDT1 (0x01, 0x25); sendDT1 (0x02, 0x25); }
        else if (paramId == "common_reverb_level")        { sendDT1 (0x01, 0x26); sendDT1 (0x02, 0x26); }
        else if (paramId == "common_pitch_bend_range")    { sendDT1 (0x01, 0x3B); sendDT1 (0x02, 0x3B); }
        else if (paramId == "common_octave_shift")        { sendDT1 (0x01, 0x3C); sendDT1 (0x02, 0x3C); }
        else if (paramId == "common_portamento_switch")   { sendDT1 (0x01, 0x3D); sendDT1 (0x02, 0x3D); }
        else if (paramId == "common_portamento_time")     { sendDT1 (0x01, 0x3E); sendDT1 (0x02, 0x3E); }
        else if (paramId == "common_mono_solo_select")    { sendDT1 (0x01, 0x3F); sendDT1 (0x02, 0x3F); }
        else if (paramId == "common_patch_level")               sendDT1 (0x00, 0x0C);
        else if (paramId == "common_keyboard_part")             sendDT1 (0x00, 0x12);
        else if (paramId == "common_split_arpeggio")            sendDT1 (0x00, 0x14);
        else if (paramId == "common_mod_destination")           sendDT1 (0x00, 0x15);
        else if (paramId == "common_dbeam_destination")         sendDT1 (0x00, 0x16);
        else if (paramId == "common_pitch_bend_destination")    sendDT1 (0x00, 0x17);
        else if (paramId == "common_expression_destination")    sendDT1 (0x00, 0x18);
        else if (paramId == "common_active_expression_switch")  sendDT1 (0x00, 0x19);
        else if (paramId == "common_arpeggio_switch")           sendDT1 (0x00, 0x1A);
        else if (paramId == "common_arpeggio_hold")             sendDT1 (0x00, 0x1B);
        else if (paramId == "common_modulation_assign")         sendDT1 (0x00, 0x1E);
        else if (paramId == "common_dbeam_assign")              sendDT1 (0x00, 0x1F);
        else if (paramId == "common_dbeam_polarity")            sendDT1 (0x00, 0x20);
        else if (paramId == "common_patch_tempo")
        {
            int val = juce::jlimit (5, 300, static_cast<int> (std::round (value)));
            uint8_t n1 = (val >> 8) & 0x0F;
            uint8_t n2 = (val >> 4) & 0x0F;
            uint8_t n3 = val & 0x0F;
            auto sendNibble = [this] (uint8_t addr3, uint8_t v) {
                uint8_t addr0 = 0x10; uint8_t addr1 = 0x00; uint8_t addr2 = 0x00;
                int sum = addr0 + addr1 + addr2 + addr3 + v;
                uint8_t cs = static_cast<uint8_t> ((128 - (sum % 128)) % 128);
                std::vector<uint8_t> pkt = { 0xF0, 0x41, 0x10, 0x00, 0x00, 0x16, 0x12, addr0, addr1, addr2, addr3, v, cs, 0xF7 };
                sendSysExToHardware (pkt);
            };
            sendNibble (0x0E, n1);
            sendNibble (0x0F, n2);
            sendNibble (0x10, n3);
        }
    }
    // Helper to send System Common DT1 SysEx message
    auto sendSystemDT1 = [this] (uint8_t addr3, uint8_t v) {
        uint8_t addr0 = 0x01; uint8_t addr1 = 0x00; uint8_t addr2 = 0x00;
        int sum = addr0 + addr1 + addr2 + addr3 + v;
        uint8_t cs = static_cast<uint8_t> ((128 - (sum % 128)) % 128);
        std::vector<uint8_t> pkt = { 0xF0, 0x41, 0x10, 0x00, 0x00, 0x16, 0x12, addr0, addr1, addr2, addr3, v, cs, 0xF7 };
        sendSysExToHardware (pkt);
    };

    if (paramId == "patch_bank_msb")               sendSystemDT1 (0x00, static_cast<uint8_t>(value));
    else if (paramId == "patch_bank_lsb")          sendSystemDT1 (0x01, static_cast<uint8_t>(value));
    else if (paramId == "patch_program_number")    sendSystemDT1 (0x02, static_cast<uint8_t>(value));
    else if (paramId == "patch_rx_tx_channel")     sendSystemDT1 (0x03, static_cast<uint8_t>(value));
    else if (paramId == "patch_remain")            sendSystemDT1 (0x04, static_cast<uint8_t>(value));
    else if (paramId == "master_tune")
    {
        int val = static_cast<int>(std::round(value * 10.0f)) + 1024;
        val = juce::jlimit(24, 2024, val);
        sendSystemDT1 (0x05, (val >> 12) & 0x0F);
        sendSystemDT1 (0x06, (val >> 8) & 0x0F);
        sendSystemDT1 (0x07, (val >> 4) & 0x0F);
        sendSystemDT1 (0x08, val & 0x0F);
    }
    else if (paramId == "master_key_shift")        sendSystemDT1 (0x09, static_cast<uint8_t>(value + 64));
    else if (paramId == "master_level")            sendSystemDT1 (0x0A, static_cast<uint8_t>(value));
    else if (paramId == "system_clock_source")     sendSystemDT1 (0x0B, static_cast<uint8_t>(value));
    else if (paramId == "system_tempo")
    {
        int val = juce::jlimit (5, 300, static_cast<int> (std::round (value)));
        sendSystemDT1 (0x0C, (val >> 8) & 0x0F);
        sendSystemDT1 (0x0D, (val >> 4) & 0x0F);
        sendSystemDT1 (0x0E, val & 0x0F);
    }
    else if (paramId == "receive_program_change")  sendSystemDT1 (0x0F, static_cast<uint8_t>(value));
    else if (paramId == "receive_bank_select")     sendSystemDT1 (0x10, static_cast<uint8_t>(value));
    else if (paramId == "transmit_edit_data")      sendSystemDT1 (0x11, static_cast<uint8_t>(value));
    else if (paramId == "soft_through")            sendSystemDT1 (0x12, static_cast<uint8_t>(value));
    else if (paramId == "midi_usb_thru")           sendSystemDT1 (0x13, static_cast<uint8_t>(value));
    else if (paramId == "transmit_program_change") sendSystemDT1 (0x14, static_cast<uint8_t>(value));
    else if (paramId == "transmit_bank_select")    sendSystemDT1 (0x15, static_cast<uint8_t>(value));
    else if (paramId == "transmit_active_sensing") sendSystemDT1 (0x16, static_cast<uint8_t>(value));
    else if (paramId == "transpose_value")         sendSystemDT1 (0x18, static_cast<uint8_t>(value + 64));
    else if (paramId == "local_switch")            sendSystemDT1 (0x19, static_cast<uint8_t>(value));
    else if (paramId == "remote_keyboard_switch")  sendSystemDT1 (0x1A, static_cast<uint8_t>(value));
    else if (paramId == "pedal_polarity")          sendSystemDT1 (0x1B, static_cast<uint8_t>(value));
    else if (paramId == "pedal_assign")            sendSystemDT1 (0x1C, static_cast<uint8_t>(value));
    else if (paramId == "d_beam_sens")             sendSystemDT1 (0x1D, static_cast<uint8_t>(value));
    else if (paramId == "recorder_sync_output")    sendSystemDT1 (0x1E, static_cast<uint8_t>(value));
    else if (paramId == "recorder_metronome_mode") sendSystemDT1 (0x1F, static_cast<uint8_t>(value));
    else if (paramId == "recorder_metronome_level") sendSystemDT1 (0x20, static_cast<uint8_t>(value));
    else if (paramId.startsWith ("patch_delay_"))
    {
        if      (paramId == "patch_delay_time")      sendDT1 (0x03, 0x00);
        else if (paramId == "patch_delay_feedback")  sendDT1 (0x03, 0x01);
        else if (paramId == "patch_delay_hf_damp")   sendDT1 (0x03, 0x02);
        else if (paramId == "patch_delay_mod_rate")  sendDT1 (0x03, 0x03);
        else if (paramId == "patch_delay_mod_depth") sendDT1 (0x03, 0x04);
    }
    else if (paramId.startsWith ("patch_reverb_"))
    {
        if      (paramId == "patch_reverb_time")         sendDT1 (0x04, 0x00);
        else if (paramId == "patch_reverb_pre_delay")    sendDT1 (0x04, 0x01);
        else if (paramId == "patch_reverb_size")         sendDT1 (0x04, 0x02);
        else if (paramId == "patch_reverb_high_cut")     sendDT1 (0x04, 0x03);
        else if (paramId == "patch_reverb_density")      sendDT1 (0x04, 0x04);
        else if (paramId == "patch_reverb_diffusion")    sendDT1 (0x04, 0x05);
        else if (paramId == "patch_reverb_lf_damp_freq") sendDT1 (0x04, 0x06);
        else if (paramId == "patch_reverb_lf_damp_gain") sendDT1 (0x04, 0x07);
        else if (paramId == "patch_reverb_hf_damp_freq") sendDT1 (0x04, 0x08);
        else if (paramId == "patch_reverb_hf_damp_gain") sendDT1 (0x04, 0x09);
    }
}

void SH201AudioProcessor::sendToneSelectToHardware (int part)
{
    currentTonePage.store (part);
    if (! isHardwareConnected())
        return;

    uint8_t addr0 = 0x10;
    uint8_t addr1 = 0x00;
    uint8_t addr2 = 0x00; // Patch Common
    uint8_t addr3 = 0x12; // Keyboard Part (0=Upper, 1=Lower)
    uint8_t val   = static_cast<uint8_t> (part ? 1 : 0);

    int sum = addr0 + addr1 + addr2 + addr3 + val;
    uint8_t checksum = static_cast<uint8_t> ((128 - (sum % 128)) % 128);

    std::vector<uint8_t> dt1Packet = {
        0xF0, 0x41, 0x10, 0x00, 0x00, 0x16, 0x12,
        addr0, addr1, addr2, addr3,
        val,
        checksum,
        0xF7
    };
    sendSysExToHardware (dt1Packet);
}

void SH201AudioProcessor::handleIncomingSysEx (const std::vector<uint8_t>& sysexData)
{
    // Check for Roland DT1 (0x12) command with minimum length (F0 41 dev 00 00 16 12 [4 addr] [val] [chk] F7)
    if (sysexData.size() < 14 || sysexData[0] != 0xF0 || sysexData[1] != 0x41 || sysexData[5] != 0x16 || sysexData[6] != 0x12)
        return;

    uint8_t addr0 = sysexData[7];
    uint8_t addr1 = sysexData[8];
    uint8_t addr2 = sysexData[9];
    uint8_t addr3 = sysexData[10];

    // Check if Address matches System Common (base 01 00 00 xx)
    if (addr0 == 0x01 && sysexData[8] == 0x00 && addr2 == 0x00)
    {
        int numDataBytes = static_cast<int> (sysexData.size()) - 13;
        if (numDataBytes <= 0)
            return;

        for (int i = 0; i < numDataBytes; ++i)
        {
            uint8_t currentOffset = static_cast<uint8_t> (addr3 + i);
            uint8_t val = sysexData[11 + i];

            if (currentOffset == 0x0B)
            {
                juce::MessageManager::callAsync ([this, val]() {
                    if (auto* param = apvts.getParameter ("system_clock_source"))
                    {
                        isUpdatingFromHardware.store (true);
                        param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (val)));
                        isUpdatingFromHardware.store (false);
                    }
                });
            }
        }
        return;
    }

    // Check if Address matches Temporary Patch, User Patch (base 10 00 xx xx or 20 00 xx xx), or System Common (01 00 00 xx)
    if (((addr0 == 0x10 || addr0 == 0x20) && (addr2 <= 0x04)) || (addr0 == 0x01 && addr1 == 0x00 && addr2 == 0x00))
    {
        int numDataBytes = static_cast<int> (sysexData.size()) - 13;
        if (numDataBytes <= 0)
            return;

        // If this is a bulk dump (multiple parameters) and we aren't explicitly syncing, ignore it.
        // This prevents delayed hardware start-up dumps from overwriting the loaded DAW project state!
        if (numDataBytes > 4 && ! isSyncingFromHardware.load())
        {
            juce::Logger::writeToLog ("SH201AudioProcessor: Ignored unrequested bulk hardware dump to protect VST state.");
            return;
        }

        for (int i = 0; i < numDataBytes; ++i)
        {
            uint8_t currentOffset = static_cast<uint8_t> (addr3 + i);
            uint8_t val = sysexData[11 + i];

            juce::String paramId;

            // Route to System Common (addr0 == 0x01)
            if (addr0 == 0x01)
            {
                if      (currentOffset == 0x00) paramId = "patch_bank_msb";
                else if (currentOffset == 0x01) paramId = "patch_bank_lsb";
                else if (currentOffset == 0x02) paramId = "patch_program_number";
                else if (currentOffset == 0x03) paramId = "patch_rx_tx_channel";
                else if (currentOffset == 0x04) paramId = "patch_remain";
                else if (currentOffset == 0x09) paramId = "master_key_shift";
                else if (currentOffset == 0x0A) paramId = "master_level";
                else if (currentOffset == 0x0B) paramId = "system_clock_source";
                else if (currentOffset == 0x0F) paramId = "receive_program_change";
                else if (currentOffset == 0x10) paramId = "receive_bank_select";
                else if (currentOffset == 0x11) paramId = "transmit_edit_data";
                else if (currentOffset == 0x12) paramId = "soft_through";
                else if (currentOffset == 0x13) paramId = "midi_usb_thru";
                else if (currentOffset == 0x14) paramId = "transmit_program_change";
                else if (currentOffset == 0x15) paramId = "transmit_bank_select";
                else if (currentOffset == 0x16) paramId = "transmit_active_sensing";
                else if (currentOffset == 0x18) paramId = "transpose_value";
                else if (currentOffset == 0x19) paramId = "local_switch";
                else if (currentOffset == 0x1A) paramId = "remote_keyboard_switch";
                else if (currentOffset == 0x1B) paramId = "pedal_polarity";
                else if (currentOffset == 0x1C) paramId = "pedal_assign";
                else if (currentOffset == 0x1D) paramId = "d_beam_sens";
                else if (currentOffset == 0x1E) paramId = "recorder_sync_output";
                else if (currentOffset == 0x1F) paramId = "recorder_metronome_mode";
                else if (currentOffset == 0x20) paramId = "recorder_metronome_level";
                else if (currentOffset == 0x05 || currentOffset == 0x06 || currentOffset == 0x07 || currentOffset == 0x08) continue; // Skip master_tune nibbles to avoid glitches, we sync from VST to hardware but not vice-versa for ease
                else if (currentOffset == 0x0C || currentOffset == 0x0D || currentOffset == 0x0E) continue; // Skip system_tempo nibbles as it's complex to rebuild from partial bytes in a stream
            }
            // Route to Patch Common (addr2 == 0x00)
            else if (addr2 == 0x00)
            {
                paramId = getPatchCommonParamIdByOffset (currentOffset);
            }
            // Route to Patch Delay (addr2 == 0x03)
            else if (addr2 == 0x03)
            {
                if      (currentOffset == 0x00) paramId = "patch_delay_time";
                else if (currentOffset == 0x01) paramId = "patch_delay_feedback";
                else if (currentOffset == 0x02) paramId = "patch_delay_hf_damp";
                else if (currentOffset == 0x03) paramId = "patch_delay_mod_rate";
                else if (currentOffset == 0x04) paramId = "patch_delay_mod_depth";
            }
            // Route to Patch Reverb (addr2 == 0x04)
            else if (addr2 == 0x04)
            {
                if      (currentOffset == 0x00) paramId = "patch_reverb_time";
                else if (currentOffset == 0x01) paramId = "patch_reverb_pre_delay";
                else if (currentOffset == 0x02) paramId = "patch_reverb_size";
                else if (currentOffset == 0x03) paramId = "patch_reverb_high_cut";
                else if (currentOffset == 0x04) paramId = "patch_reverb_density";
                else if (currentOffset == 0x05) paramId = "patch_reverb_diffusion";
                else if (currentOffset == 0x06) paramId = "patch_reverb_lf_damp_freq";
                else if (currentOffset == 0x07) paramId = "patch_reverb_lf_damp_gain";
                else if (currentOffset == 0x08) paramId = "patch_reverb_hf_damp_freq";
                else if (currentOffset == 0x09) paramId = "patch_reverb_hf_damp_gain";
            }
            // Route to Tone Memory (addr2 == 0x01 Upper or 0x02 Lower)
            else if (addr2 == 0x01 || addr2 == 0x02)
            {
                // Check if offset is a Common Performance / Master Effect parameter
                if      (currentOffset == 0x25) paramId = "common_delay_level";
                else if (currentOffset == 0x26) paramId = "common_reverb_level";
                else if (currentOffset == 0x3B) paramId = "common_pitch_bend_range";
                else if (currentOffset == 0x3C) paramId = "common_octave_shift";
                else if (currentOffset == 0x3D) paramId = "common_portamento_switch";
                else if (currentOffset == 0x3E) paramId = "common_portamento_time";
                else if (currentOffset == 0x3F) paramId = "common_mono_solo_select";
                else
                {
                    juce::String baseId = getBaseParamIdByToneOffset (currentOffset);
                    if (baseId.isNotEmpty())
                        paramId = (addr2 == 0x01 ? "upper_" : "lower_") + baseId;
                }
            }

            if (paramId.isNotEmpty())
            {
                if (numDataBytes <= 4)
                {
                    std::lock_guard<std::mutex> lock (txTimeMutex);
                    auto txIt = lastHardwareTxTime.find (paramId);
                    if (txIt != lastHardwareTxTime.end())
                    {
                        uint32_t now = juce::Time::getMillisecondCounter();
                        if ((now - txIt->second) < 500)
                            continue; // Suppress echo from our own transmission!
                    }
                }

                if (auto* param = apvts.getParameter (paramId))
                {
                    {
                        std::lock_guard<std::mutex> lock (rxTimeMutex);
                        lastHardwareRxTime[paramId] = juce::Time::getMillisecondCounter();
                    }

                    float normalized = 0.0f;
                    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (param))
                    {
                        int numChoices = choiceParam->choices.size();
                        normalized = juce::jlimit (0, numChoices - 1, static_cast<int> (val)) / static_cast<float> (juce::jmax (1, numChoices - 1));
                    }
                    else if (auto* intParam = dynamic_cast<juce::AudioParameterInt*> (param))
                    {
                        int minVal = intParam->getRange().getStart();
                        int maxVal = intParam->getRange().getEnd();
                        int decodedInt = static_cast<int> (val);
                        if (paramId.endsWith ("filter_keyfollow"))
                            decodedInt = (static_cast<int> (val) - 64) * 10;
                        else if (minVal < 0)
                        {
                            decodedInt = static_cast<int> (val) - 64;
                            if (paramId == "common_tone_balance")
                            {
                                decodedInt = -decodedInt;
                            }
                            if (paramId.endsWith ("osc1_pitch"))
                            {
                                auto* wideParam = apvts.getRawParameterValue (paramId.startsWith ("upper_") ? "upper_osc1_pitch_wide" : "lower_osc1_pitch_wide");
                                if (wideParam != nullptr && *wideParam <= 0.5f)
                                    decodedInt /= 3;
                            }
                            else if (paramId.endsWith ("osc2_pitch"))
                            {
                                auto* wideParam = apvts.getRawParameterValue (paramId.startsWith ("upper_") ? "upper_osc2_pitch_wide" : "lower_osc2_pitch_wide");
                                if (wideParam != nullptr && *wideParam <= 0.5f)
                                    decodedInt /= 3;
                            }
                        }

                        normalized = intParam->convertTo0to1 (juce::jlimit (minVal, maxVal, decodedInt));
                    }
                    else
                    {
                        normalized = juce::jlimit (0.0f, 1.0f, val / 127.0f);
                    }

                    if (std::abs (param->getValue() - normalized) > 0.0001f)
                    {
                        markParameterUpdatedFromHardware (paramId);
                        lastHardwareKnobActivityTime.store (juce::Time::getMillisecondCounter());

                        // Dispatch to MessageManager to ensure thread-safe GUI repaint and DAW host automation notification
                        juce::MessageManager::callAsync ([this, paramId, normalized]() {
                            if (auto* p = apvts.getParameter (paramId))
                            {
                                markParameterUpdatedFromHardware (paramId);
                                isUpdatingFromHardware.store (true);
                                p->setValueNotifyingHost (normalized);
                                isUpdatingFromHardware.store (false);
                            }
                        });
                    }
                }
            }
        }
        juce::Logger::writeToLog ("SH201: Successfully parsed DT1 SysEx patch data (" + juce::String (numDataBytes) + " bytes).");
        
        if (addr2 == 0x04)
        {
            isSyncingFromHardware.store (false);
        }
    }
}

void SH201AudioProcessor::handleIncomingMidiCC (uint8_t channel, uint8_t ccNum, uint8_t value)
{
    juce::String baseId;
    if      (ccNum == 74 || ccNum == 102) baseId = "filter_cutoff";
    else if (ccNum == 71 || ccNum == 104) baseId = "filter_resonance";
    else if (ccNum == 73 || ccNum == 110) baseId = "amp_attack";
    else if (ccNum == 75 || ccNum == 111) baseId = "amp_decay";
    else if (ccNum == 79 || ccNum == 112 || ccNum == 31) baseId = "amp_sustain";
    else if (ccNum == 72 || ccNum == 113) baseId = "amp_release";
    else if (ccNum == 82 || ccNum == 106) baseId = "filter_attack";
    else if (ccNum == 83 || ccNum == 107) baseId = "filter_decay";
    else if (ccNum == 85 || ccNum == 108 || ccNum == 28) baseId = "filter_sustain";
    else if (ccNum == 88 || ccNum == 109 || ccNum == 29) baseId = "filter_release";
    else if (ccNum == 81 || ccNum == 105) baseId = "filter_env_depth";
    else if (ccNum == 30)                 baseId = "filter_keyfollow";
    else if (ccNum == 16)                 baseId = "lfo1_rate";
    else if (ccNum == 18)                 baseId = "lfo1_depth";
    else if (ccNum == 19)                 baseId = "lfo1_depth2";
    else if (ccNum == 17)                 baseId = "lfo2_rate";
    else if (ccNum == 22)                 baseId = "lfo2_depth";
    else if (ccNum == 23)                 baseId = "lfo2_depth2";
    else if (ccNum == 91 || ccNum == 116) baseId = "reverb_level";
    else if (ccNum == 13)                 baseId = "reverb_time";
    else if (ccNum == 93)                 baseId = "delay_level";
    else if (ccNum == 12)                 baseId = "delay_time";
    else if (ccNum == 118)                baseId = "overdrive_level";
    else if (ccNum == 70 || ccNum == 100) baseId = "osc1_wave";
    else if (ccNum == 20)                 baseId = "osc1_pitch";
    else if (ccNum == 76)                 baseId = "osc1_fine_tune";
    else if (ccNum == 3)                  baseId = "osc1_pulse_width";
    else if (ccNum == 24)                 baseId = "osc1_pitch_env_depth";
    else if (ccNum == 21)                 baseId = "osc2_pitch";
    else if (ccNum == 77)                 baseId = "osc2_detune";
    else if (ccNum == 95)                 baseId = "osc2_pulse_width";
    else if (ccNum == 25)                 baseId = "osc2_pitch_env_depth";
    else if (ccNum == 26)                 baseId = "pitch_env_attack";
    else if (ccNum == 27)                 baseId = "pitch_env_decay";
    else if (ccNum == 8)                  baseId = "mix_mod_balance";
    else if (ccNum == 14)                 baseId = "amp_level";
    else if (ccNum == 10)                 baseId = "amp_pan";
    else if (ccNum == 5)                  baseId = "portamento_time";
    else if (ccNum == 65)                 baseId = "portamento_switch";

    lastRxCC.store (ccNum);
    lastRxVal.store (value);

    juce::String paramId;
    if (baseId.isNotEmpty())
    {
        if      (baseId == "reverb_level")      paramId = "common_reverb_level";
        else if (baseId == "delay_level")       paramId = "common_delay_level";
        else if (baseId == "portamento_time")   paramId = "common_portamento_time";
        else if (baseId == "portamento_switch") paramId = "common_portamento_switch";
        else if (baseId == "reverb_time")       paramId = "patch_reverb_time";
        else if (baseId == "delay_time")        paramId = "patch_delay_time";
        else                                    paramId = ((channel & 0x0F) == 1 ? "lower_" : "upper_") + baseId;

        setLastRxParamName (paramId);

        if (auto* param = apvts.getParameter (paramId))
        {
            {
                std::lock_guard<std::mutex> lock (txTimeMutex);
                auto txIt = lastHardwareTxTime.find (paramId);
                if (txIt != lastHardwareTxTime.end())
                {
                    uint32_t now = juce::Time::getMillisecondCounter();
                    if ((now - txIt->second) < 500)
                    {
                        // We recently sent this parameter to the hardware. 
                        // This incoming CC is an echo. Ignore it to prevent MIDI loops!
                        return;
                    }
                }
            }

            {
                std::lock_guard<std::mutex> lock (rxTimeMutex);
                lastHardwareRxTime[paramId] = juce::Time::getMillisecondCounter();
            }

            float normalized = 0.0f;
            if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (param))
            {
                int numChoices = choiceParam->choices.size();
                normalized = juce::jlimit (0, numChoices - 1, static_cast<int> (value)) / static_cast<float> (juce::jmax (1, numChoices - 1));
            }
            else if (auto* intParam = dynamic_cast<juce::AudioParameterInt*> (param))
            {
                int minVal = intParam->getRange().getStart();
                int maxVal = intParam->getRange().getEnd();
                int decodedInt = static_cast<int> (value);
                if (paramId.endsWith ("filter_keyfollow"))
                    decodedInt = (static_cast<int> (value) - 64) * 10;
                else if (minVal < 0)
                {
                    decodedInt = static_cast<int> (value) - 64;
                    if (paramId == "common_tone_balance")
                    {
                        decodedInt = -decodedInt;
                    }
                    if (paramId.endsWith ("osc1_pitch"))
                    {
                        auto* wideParam = apvts.getRawParameterValue (paramId.startsWith ("upper_") ? "upper_osc1_pitch_wide" : "lower_osc1_pitch_wide");
                        if (wideParam != nullptr && *wideParam <= 0.5f)
                            decodedInt /= 3;
                    }
                    else if (paramId.endsWith ("osc2_pitch"))
                    {
                        auto* wideParam = apvts.getRawParameterValue (paramId.startsWith ("upper_") ? "upper_osc2_pitch_wide" : "lower_osc2_pitch_wide");
                        if (wideParam != nullptr && *wideParam <= 0.5f)
                            decodedInt /= 3;
                    }
                }
                normalized = intParam->convertTo0to1 (juce::jlimit (minVal, maxVal, decodedInt));
            }
            else
            {
                normalized = juce::jlimit (0.0f, 1.0f, value / 127.0f);
            }

            lastHardwareKnobActivityTime.store (juce::Time::getMillisecondCounter());

            // Dispatch to MessageManager to ensure thread-safe GUI repaint and DAW host automation notification
            juce::MessageManager::callAsync ([this, paramId, normalized]() {
                if (auto* p = apvts.getParameter (paramId))
                {
                    {
                        std::lock_guard<std::mutex> lock (rxTimeMutex);
                        lastHardwareRxTime[paramId] = juce::Time::getMillisecondCounter();
                    }
                    isUpdatingFromHardware.store (true);
                    p->setValueNotifyingHost (normalized);
                    isUpdatingFromHardware.store (false);
                }
            });

            juce::Logger::writeToLog ("SH201: Hardware MIDI CC " + juce::String (ccNum) +
                                      " -> Param: " + paramId + " = " + juce::String (value));
        }
    }
    else
    {
        setLastRxParamName ("CC#" + juce::String (ccNum));
    }
}

const juce::String SH201AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SH201AudioProcessor::acceptsMidi() const
{
    return true;
}

bool SH201AudioProcessor::producesMidi() const
{
    return true;
}

bool SH201AudioProcessor::isMidiEffect() const
{
    return false;
}

double SH201AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SH201AudioProcessor::getNumPrograms()
{
    return 1;
}

int SH201AudioProcessor::getCurrentProgram()
{
    return 0;
}

void SH201AudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String SH201AudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return "Roland SH-201 Editor";
}

void SH201AudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void SH201AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::Logger::writeToLog ("SH201AudioProcessor: prepareToPlay called (SampleRate: " +
                              juce::String (sampleRate) + ", BlockSize: " +
                              juce::String (samplesPerBlock) + ")");
    
    if (usbManager)
        usbManager->setDawSampleRate (sampleRate);
}

void SH201AudioProcessor::releaseResources()
{
}

bool SH201AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo() &&
        layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;

    return true;
}

static bool isSH201SoundEditCC (int ccNum) noexcept
{
    switch (ccNum)
    {
        case 3:   // osc1_pulse_width
        case 5:   // portamento_time
        case 8:   // mix_mod_balance
        case 10:  // amp_pan
        case 12:  // delay_time
        case 13:  // reverb_time
        case 14:  // amp_level
        case 16:  // lfo1_rate
        case 17:  // lfo2_rate
        case 18:  // lfo1_depth
        case 19:  // lfo1_depth2
        case 20:  // osc1_pitch
        case 21:  // osc2_pitch
        case 22:  // lfo2_depth
        case 23:  // lfo2_depth2
        case 24:  // osc1_pitch_env_depth
        case 25:  // osc2_pitch_env_depth
        case 26:  // pitch_env_attack
        case 27:  // pitch_env_decay
        case 28:  // filter_sustain
        case 29:  // filter_release
        case 30:  // filter_keyfollow
        case 31:  // amp_sustain
        case 65:  // portamento_switch
        case 70:  // osc1_wave
        case 71:  // filter_resonance
        case 72:  // amp_release
        case 73:  // amp_attack
        case 74:  // filter_cutoff
        case 75:  // amp_decay
        case 76:  // osc1_fine_tune
        case 77:  // osc2_detune
        case 79:  // amp_sustain
        case 81:  // filter_env_depth
        case 82:  // filter_attack
        case 83:  // filter_decay
        case 85:  // filter_sustain
        case 88:  // filter_release
        case 91:  // reverb_level
        case 93:  // delay_level
        case 95:  // osc2_pulse_width
        case 100: // osc1_wave
        case 102: // filter_cutoff
        case 104: // filter_resonance
        case 105: // filter_env_depth
        case 106: // filter_attack
        case 107: // filter_decay
        case 108: // filter_sustain
        case 109: // filter_release
        case 110: // amp_attack
        case 111: // amp_decay
        case 112: // amp_sustain
        case 113: // amp_release
        case 116: // reverb_level
        case 118: // overdrive_level
            return true;
        default:
            return false;
    }
}

void SH201AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // Phase 1 fix: Use lock-free FIFO queue instead of synchronous USB I/O on the real-time audio thread.
    // This eliminates the DPC_WATCHDOG_VIOLATION BSOD caused by blocking mutex + kernel USB calls in processBlock.
    if (isHardwareConnected())
    {
        syncHostTempoAndClock (getPlayHead(), buffer.getNumSamples());

        for (const auto metadata : midiMessages)
        {
            auto msg = metadata.getMessage();
            // Filter out front-panel sound edit CCs to prevent DAW track MIDI Thru feedback loops
            if (msg.isController() && isSH201SoundEditCC (msg.getControllerNumber()))
                continue;

            queueMidiMessageToHardware (msg);
        }
    }

    if (isAudioStreamingActive())
    {
        usbManager->readAudioFromUSB (buffer, buffer.getNumSamples());
    }
    else
    {
        buffer.clear();
    }
}

bool SH201AudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* SH201AudioProcessor::createEditor()
{
    return new SH201AudioProcessorEditor (*this);
}

void SH201AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SH201AudioProcessor::syncAllParametersToHardware()
{
    // Phase 4 fix: Use juce::Timer one-shot instead of detached juce::Thread::launch with unsafe [this] capture.
    // The timer callback runs on the message thread, which is safe for APVTS access and hardware manager calls.
    juce::Timer::callAfterDelay (3000, [safeThis = juce::Component::SafePointer<juce::Component> (nullptr), this]() {
        // Verify and refresh connection if not currently connected
        if (! isHardwareConnected())
        {
            juce::Logger::writeToLog ("SH201AudioProcessor: Connection not active during patch sync. Refreshing connection...");
            reinitializeHardwareConnection();
        }

        if (! isHardwareConnected())
        {
            juce::Logger::writeToLog ("SH201AudioProcessor: Could not connect to SH-201 hardware for project patch sync.");
            return;
        }

        juce::Logger::writeToLog ("SH201AudioProcessor: Async syncing saved project patch state to SH-201 hardware...");
        int delayMs = 10;

        // Skip patch selection parameters during bulk sync because sending them causes the hardware
        // to load a preset from memory and echo a full SysEx dump back to the VST, which overwrites
        // the custom VST project state with the default preset state!
        juce::StringArray skipParams = { "patch_bank_msb", "patch_bank_lsb", "patch_program_number" };

        for (auto* param : getParameters())
        {
            if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
            {
                if (skipParams.contains (p->paramID))
                    continue; // Skip patch selection to avoid destructive hardware echo

                float rawVal = p->getValue();
                if (auto* rawPtr = apvts.getRawParameterValue (p->paramID))
                    rawVal = rawPtr->load();

                juce::String paramID = p->paramID;
                juce::Timer::callAfterDelay (delayMs, [this, paramID, rawVal]() {
                    sendParameterChangeToHardware (paramID, rawVal);
                });
                delayMs += 10; // Space out each parameter by 10ms to prevent overwhelming the synth's MIDI buffer
            }
        }
    });
}

void SH201AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    hasLoadedDAWProjectState.store (true);
    isSyncingFromHardware.store (false); // Abort any ongoing startup sync to protect project state
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
    {
        juce::Logger::writeToLog ("SH201AudioProcessor: Project loaded saved state. Verifying USB connection and restoring synth patch...");
        
        isUpdatingFromHardware.store (true);
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));

        // Ensure backwards compatibility for older DAW projects saved before upper_/lower_/common_ prefix architecture
        auto migrateLegacyParam = [this, &xmlState] (const juce::String& legacyId, const juce::String& newId) {
            if (auto* newP = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (newId)))
            {
                if (auto* child = xmlState->getChildByAttribute ("id", legacyId))
                {
                    float val = (float) child->getDoubleAttribute ("value", 0.0);
                    newP->setValueNotifyingHost (newP->convertTo0to1 (val));
                }
            }
        };
        for (const auto& baseId : getBaseToneParamIds())
            migrateLegacyParam (baseId, juce::String ("upper_") + baseId);

        migrateLegacyParam ("reverb_level", "common_reverb_level");
        migrateLegacyParam ("delay_level", "common_delay_level");
        migrateLegacyParam ("pitch_bend_range", "common_pitch_bend_range");
        migrateLegacyParam ("octave_shift", "common_octave_shift");
        migrateLegacyParam ("portamento_switch", "common_portamento_switch");
        migrateLegacyParam ("portamento_time", "common_portamento_time");
        migrateLegacyParam ("mono_solo_select", "common_mono_solo_select");

        // Force UI and host notification for all recalled parameters so UI knobs update immediately
        for (auto* param : getParameters())
        {
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
            {
                if (auto* rawPtr = apvts.getRawParameterValue (p->paramID))
                {
                    float rawVal = rawPtr->load();
                    p->setValueNotifyingHost (p->convertTo0to1 (rawVal));
                }
            }
        }
        isUpdatingFromHardware.store (false);

        syncAllParametersToHardware();
    }
}

bool SH201AudioProcessor::isStereoModeActive()
{
    if (auto* rawPtr = apvts.getRawParameterValue ("common_stereo_mode"))
    {
        return (static_cast<int> (std::round (rawPtr->load())) == 1);
    }
    return false;
}

bool SH201AudioProcessor::isStereoExcludeOscControls()
{
    if (auto* rawPtr = apvts.getRawParameterValue ("common_stereo_link_mode"))
    {
        return (static_cast<int> (std::round (rawPtr->load())) == 1);
    }
    return false;
}

void SH201AudioProcessor::activateStereoMode()
{
    juce::Logger::writeToLog ("SH201AudioProcessor: Activating Stereo Mode...");

    juce::String srcPrefix = (currentTonePage.load() == 0) ? "upper_" : "lower_";
    juce::String dstPrefix = (currentTonePage.load() == 0) ? "lower_" : "upper_";
    bool excludeOsc = isStereoExcludeOscControls();

    isUpdatingStereoMirror.store (true);

    std::vector<std::pair<juce::String, float>> toSyncHardware;

    // 1. Copy patch from current tone to opposite tone
    for (const auto& baseId : getBaseToneParamIds())
    {
        if (baseId == "amp_pan")
            continue;

        if (excludeOsc && (baseId.startsWith ("osc1_") || baseId.startsWith ("osc2_")))
            continue;

        if (auto* srcParam = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (srcPrefix + baseId)))
        {
            if (auto* dstParam = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (dstPrefix + baseId)))
            {
                float normVal = srcParam->getValue();
                float rawVal = srcParam->convertFrom0to1 (normVal);
                if (auto* rawPtr = apvts.getRawParameterValue (srcPrefix + baseId))
                    rawVal = rawPtr->load();

                // Always set both so UI and APVTS are guaranteed to update
                markParameterUpdatedFromHardware (dstPrefix + baseId);
                dstParam->setValueNotifyingHost (normVal);
                dstParam->setValue (normVal);

                toSyncHardware.push_back ({ dstPrefix + baseId, rawVal });
            }
        }
    }

    // 2. Switch Keyboard Mode to DUAL (index 1 in { "SINGLE", "DUAL", "SPLIT" })
    if (auto* kbModeParam = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter ("common_keyboard_mode")))
    {
        float targetNorm = kbModeParam->convertTo0to1 (1.0f);
        kbModeParam->setValueNotifyingHost (targetNorm);
        kbModeParam->setValue (targetNorm);
    }

    isUpdatingStereoMirror.store (false);

    // 3. Apply Stereo Width control to tone pans
    applyStereoWidth();

    // 4. Send bulk tone DT1 packets with pacing so the SH-201 hardware USB MIDI buffer never drops messages or floods
    juce::Timer::callAfterDelay (35, [this]() {
        auto upperPkt = SH201PatchSerializer::createToneDT1Packet (*this, "upper_", 0x01);
        sendSysExToHardware (upperPkt);
    });

    juce::Timer::callAfterDelay (65, [this]() {
        auto lowerPkt = SH201PatchSerializer::createToneDT1Packet (*this, "lower_", 0x02);
        sendSysExToHardware (lowerPkt);
    });
}

void SH201AudioProcessor::deactivateStereoMode()
{
    juce::Logger::writeToLog ("SH201AudioProcessor: Deactivating Stereo Mode...");

    // 1. Switch Keyboard Mode back to SINGLE (index 0)
    if (auto* kbModeParam = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter ("common_keyboard_mode")))
    {
        float targetNorm = kbModeParam->convertTo0to1 (0.0f);
        kbModeParam->setValueNotifyingHost (targetNorm);
        kbModeParam->setValue (targetNorm);
    }

    // 2. Set both Tone Pan amounts to 0 (center = 0 in -64..+63 range)
    if (auto* upperPanParam = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter ("upper_amp_pan")))
    {
        float targetNorm = upperPanParam->convertTo0to1 (0.0f);
        upperPanParam->setValueNotifyingHost (targetNorm);
        upperPanParam->setValue (targetNorm);
    }

    if (auto* lowerPanParam = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter ("lower_amp_pan")))
    {
        float targetNorm = lowerPanParam->convertTo0to1 (0.0f);
        lowerPanParam->setValueNotifyingHost (targetNorm);
        lowerPanParam->setValue (targetNorm);
    }
}

void SH201AudioProcessor::applyStereoWidth()
{
    float widthVal = 100.0f;
    if (auto* wPtr = apvts.getRawParameterValue ("common_stereo_width"))
        widthVal = wPtr->load();

    float w = juce::jlimit (0.0f, 100.0f, widthVal) / 100.0f;

    int upperPan = juce::jlimit (-64, 0, juce::roundToInt (-64.0f * w));
    int lowerPan = juce::jlimit (0, 63, juce::roundToInt (63.0f * w));

    isUpdatingStereoMirror.store (true);
    if (auto* upperPanParam = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter ("upper_amp_pan")))
    {
        float targetNorm = upperPanParam->convertTo0to1 (static_cast<float> (upperPan));
        if (std::abs (upperPanParam->getValue() - targetNorm) > 0.0001f)
        {
            upperPanParam->setValueNotifyingHost (targetNorm);
        }
    }
    if (auto* lowerPanParam = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter ("lower_amp_pan")))
    {
        float targetNorm = lowerPanParam->convertTo0to1 (static_cast<float> (lowerPan));
        if (std::abs (lowerPanParam->getValue() - targetNorm) > 0.0001f)
        {
            lowerPanParam->setValueNotifyingHost (targetNorm);
        }
    }
    isUpdatingStereoMirror.store (false);
}

void SH201AudioProcessor::handleStereoModeParameterChange (const juce::String& paramId, float newValue)
{
    if (isUpdatingStereoMirror.load())
        return;

    if (paramId == "common_stereo_mode")
    {
        if (isStereoModeActive())
            activateStereoMode();
        else
            deactivateStereoMode();
        return;
    }

    if (paramId == "common_stereo_width")
    {
        if (isStereoModeActive())
            applyStereoWidth();
        return;
    }

    if (paramId == "common_stereo_link_mode")
    {
        if (isStereoModeActive())
            activateStereoMode();
        return;
    }

    if (! isStereoModeActive())
        return;

    juce::String baseId;
    juce::String targetParamId;

    if (paramId.startsWith ("upper_"))
    {
        baseId = paramId.substring (6);
        targetParamId = "lower_" + baseId;
    }
    else if (paramId.startsWith ("lower_"))
    {
        baseId = paramId.substring (6);
        targetParamId = "upper_" + baseId;
    }
    else
    {
        return;
    }

    if (baseId == "amp_pan")
        return;

    if (isStereoExcludeOscControls() && (baseId.startsWith ("osc1_") || baseId.startsWith ("osc2_")))
        return;

    if (auto* srcParam = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (paramId)))
    {
        if (auto* dstParam = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (targetParamId)))
        {
            float normVal = srcParam->convertTo0to1 (newValue);
            if (std::abs (dstParam->getValue() - normVal) > 0.0001f)
            {
                if (isUpdatingFromHardware.load())
                {
                    std::lock_guard<std::mutex> lock (txTimeMutex);
                    pendingMirroredHardwareUpdates[targetParamId] = newValue;
                }

                isUpdatingStereoMirror.store (true);
                dstParam->setValueNotifyingHost (normVal);
                isUpdatingStereoMirror.store (false);
            }
        }
    }
}

void SH201AudioProcessor::handleLinkModeParameterChange (const juce::String& paramId, float newValue)
{
    if (isUpdatingStereoMirror.load())
        return;

    // Handle initial one-time sync when Link is toggled ON
    if (paramId.startsWith ("link_"))
    {
        if (newValue > 0.5f)
        {
            juce::String srcPrefix = (currentTonePage.load() == 0) ? "upper_" : "lower_";
            juce::String dstPrefix = (currentTonePage.load() == 0) ? "lower_" : "upper_";
            
            std::vector<juce::String> paramsToSync;
            if (paramId == "link_osc1") paramsToSync = { "osc1_wave", "osc1_pitch_wide", "osc1_pitch", "osc1_fine_tune", "osc1_pulse_width", "osc1_pitch_env_depth" };
            else if (paramId == "link_osc2") paramsToSync = { "osc2_wave", "osc2_pitch_wide", "osc2_pitch", "osc2_detune", "osc2_pulse_width", "osc2_pitch_env_depth" };
            else if (paramId == "link_mix_mod") paramsToSync = { "mix_mod_type", "mix_mod_balance", "mix_mod_low_freq" };
            else if (paramId == "link_pitch_env") paramsToSync = { "pitch_env_attack", "pitch_env_decay" };
            else if (paramId == "link_filter") paramsToSync = { "filter_type", "filter_slope", "filter_cutoff", "filter_resonance", "filter_keyfollow", "filter_velocity_sens" };
            else if (paramId == "link_filter_env") paramsToSync = { "filter_attack", "filter_decay", "filter_sustain", "filter_release", "filter_env_depth" };
            else if (paramId == "link_amp") paramsToSync = { "amp_level", "amp_pan", "amp_velocity_sens", "amp_overdrive_switch", "overdrive_level" };
            else if (paramId == "link_amp_env") paramsToSync = { "amp_attack", "amp_decay", "amp_sustain", "amp_release" };
            else if (paramId == "link_lfo1") paramsToSync = { "lfo1_shape", "lfo1_rate", "lfo1_tempo_sync", "lfo1_tempo_note", "lfo1_fade_time", "lfo1_key_trigger", "lfo1_dest1", "lfo1_depth", "lfo1_dest2", "lfo1_depth2" };
            else if (paramId == "link_lfo2") paramsToSync = { "lfo2_shape", "lfo2_rate", "lfo2_tempo_sync", "lfo2_tempo_note", "lfo2_fade_time", "lfo2_key_trigger", "lfo2_dest1", "lfo2_depth", "lfo2_dest2", "lfo2_depth2" };

            isUpdatingStereoMirror.store (true);
            for (const auto& baseId : paramsToSync)
            {
                if (auto* srcParam = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (srcPrefix + baseId)))
                {
                    if (auto* dstParam = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (dstPrefix + baseId)))
                    {
                        float normVal = srcParam->getValue();
                        if (std::abs (dstParam->getValue() - normVal) > 0.0001f)
                            dstParam->setValueNotifyingHost (normVal);
                    }
                }
            }
            isUpdatingStereoMirror.store (false);
        }
        return;
    }

    if (!paramId.startsWith ("upper_") && !paramId.startsWith ("lower_"))
        return;

    juce::String baseId;
    juce::String targetParamId;

    if (paramId.startsWith ("upper_"))
    {
        baseId = paramId.substring (6);
        targetParamId = "lower_" + baseId;
    }
    else
    {
        baseId = paramId.substring (6);
        targetParamId = "upper_" + baseId;
    }

    juce::String linkParamId;
    if (baseId.startsWith ("osc1_")) linkParamId = "link_osc1";
    else if (baseId.startsWith ("osc2_")) linkParamId = "link_osc2";
    else if (baseId.startsWith ("mix_mod_")) linkParamId = "link_mix_mod";
    else if (baseId.startsWith ("pitch_env_")) linkParamId = "link_pitch_env";
    else if (baseId == "filter_type" || baseId == "filter_slope" || baseId == "filter_cutoff" || baseId == "filter_resonance" || baseId == "filter_keyfollow" || baseId == "filter_velocity_sens") linkParamId = "link_filter";
    else if (baseId == "filter_attack" || baseId == "filter_decay" || baseId == "filter_sustain" || baseId == "filter_release" || baseId == "filter_env_depth") linkParamId = "link_filter_env";
    else if (baseId == "amp_level" || baseId == "amp_pan" || baseId == "amp_velocity_sens" || baseId == "amp_overdrive_switch" || baseId == "overdrive_level") linkParamId = "link_amp";
    else if (baseId == "amp_attack" || baseId == "amp_decay" || baseId == "amp_sustain" || baseId == "amp_release") linkParamId = "link_amp_env";
    else if (baseId.startsWith ("lfo1_")) linkParamId = "link_lfo1";
    else if (baseId.startsWith ("lfo2_")) linkParamId = "link_lfo2";

    if (linkParamId.isNotEmpty())
    {
        if (auto* linkParam = apvts.getRawParameterValue (linkParamId))
        {
            if (static_cast<int>(std::round(linkParam->load())) == 1)
            {
                // Verify we're not also doing this via stereo mode mirroring right now
                if (isStereoModeActive())
                {
                    if (linkParamId == "link_osc1" || linkParamId == "link_osc2")
                    {
                        if (!isStereoExcludeOscControls())
                            return; // Handled by stereo mode
                    }
                    else if (baseId != "amp_pan")
                    {
                        return; // Handled by stereo mode
                    }
                }

                if (auto* srcParam = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (paramId)))
                {
                    if (auto* dstParam = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (targetParamId)))
                    {
                        float normVal = srcParam->convertTo0to1 (newValue);
                        if (std::abs (dstParam->getValue() - normVal) > 0.0001f)
                        {
                            if (isUpdatingFromHardware.load())
                            {
                                std::lock_guard<std::mutex> lock (txTimeMutex);
                                pendingMirroredHardwareUpdates[targetParamId] = newValue;
                            }

                            isUpdatingStereoMirror.store (true);
                            dstParam->setValueNotifyingHost (normVal);
                            isUpdatingStereoMirror.store (false);
                        }
                    }
                }
            }
        }
    }
}

void SH201AudioProcessor::syncHostTempoAndClock (juce::AudioPlayHead* playHead, int numSamples)
{
    if (playHead == nullptr || ! isHardwareConnected())
        return;

    auto positionInfo = playHead->getPosition();
    if (! positionInfo.hasValue())
        return;

    // 1. Host BPM -> SysEx Tempo Sync (for PATCH or SYSTEM Clock Mode)
    if (positionInfo->getBpm().hasValue())
    {
        double hostBpm = *positionInfo->getBpm();
        if (hostBpm >= 5.0 && hostBpm <= 300.0 && std::abs (hostBpm - lastHostBpm) >= 0.5)
        {
            lastHostBpm = hostBpm;
            int intBpm = juce::jlimit (5, 300, static_cast<int> (std::round (hostBpm)));
            juce::MessageManager::callAsync ([safeThis = juce::WeakReference<SH201AudioProcessor> (this), intBpm]() {
                if (safeThis != nullptr)
                {
                    if (auto* param = safeThis->apvts.getParameter ("common_patch_tempo"))
                        param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (intBpm)));
                    if (auto* sysParam = safeThis->apvts.getParameter ("system_tempo"))
                        sysParam->setValueNotifyingHost (sysParam->convertTo0to1 (static_cast<float> (intBpm)));
                }
            });
        }
    }

    // 2. Real-Time 24 PPQ MIDI Clock Generator (for USB / MIDI Clock Mode)
    bool isPlaying = positionInfo->getIsPlaying();

    if (! wasPlaying && isPlaying)
    {
        // Playback just started -> send MIDI Start (0xFA)
        queueMidiMessageToHardware (juce::MidiMessage::midiStart());
        if (positionInfo->getPpqPosition().hasValue())
            lastClockPulse = static_cast<int64_t> (std::floor (*positionInfo->getPpqPosition() * 24.0)) - 1;
        else
            lastClockPulse = -1;
    }
    else if (wasPlaying && ! isPlaying)
    {
        // Playback just stopped -> send MIDI Stop (0xFC)
        queueMidiMessageToHardware (juce::MidiMessage::midiStop());
        lastClockPulse = -1;
    }
    wasPlaying = isPlaying;

    if (isPlaying && positionInfo->getPpqPosition().hasValue() && positionInfo->getBpm().hasValue())
    {
        double currentPpq = *positionInfo->getPpqPosition();
        double bpm = *positionInfo->getBpm();
        double sampleRate = getSampleRate();

        if (sampleRate > 0.0 && bpm > 0.0)
        {
            double ppqPerSample = (bpm / 60.0) / sampleRate;
            double endPpq = currentPpq + (numSamples * ppqPerSample);

            int64_t currentPulse = static_cast<int64_t> (std::floor (endPpq * 24.0));
            if (lastClockPulse < 0 || currentPulse < lastClockPulse - 48)
                lastClockPulse = static_cast<int64_t> (std::floor (currentPpq * 24.0)) - 1;

            while (lastClockPulse < currentPulse)
            {
                queueMidiMessageToHardware (juce::MidiMessage::midiClock());
                lastClockPulse++;
            }
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SH201AudioProcessor();
}
