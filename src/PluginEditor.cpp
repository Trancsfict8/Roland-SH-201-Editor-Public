#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Utils/GlobalSettings.h"

class SystemSettingsComponent : public juce::Component
{
public:
    SystemSettingsComponent (SH201AudioProcessorEditor& editor, SH201AudioProcessor& p, SH201LookAndFeel& lf)
    {
        auto setupSlider = [this, &p, &lf] (const juce::String& paramId, const juce::String& name)
        {
            auto* s = sliders.add (new juce::Slider());
            auto* l = labels.add (new juce::Label());
            s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s->setLookAndFeel (&lf);
            s->getProperties().set ("isVectorKnob", true);
            s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 54, 18);
            s->setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
            s->setColour (juce::Slider::thumbColourId, juce::Colour (0xff8893a0));
            s->setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffffffff));
            s->setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff999999));
            addAndMakeVisible (s);
            
            l->setText (name, juce::dontSendNotification);
            l->setColour (juce::Label::textColourId, juce::Colours::white);
            l->setJustificationType (juce::Justification::centred);
            l->setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::bold));
            addAndMakeVisible (l);
            
            sliderAttachments.add (new juce::AudioProcessorValueTreeState::SliderAttachment (p.getAPVTS(), paramId, *s));
            sliderMap.push_back ({ s, l, paramId });
        };

        auto setupCombo = [this, &p] (const juce::String& paramId, const juce::String& name, const juce::StringArray& items)
        {
            auto* c = combos.add (new juce::ComboBox());
            auto* l = labels.add (new juce::Label());
            c->addItemList (items, 1);
            c->setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2e333d));
            c->setColour (juce::ComboBox::textColourId, juce::Colours::white);
            c->setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff555555));
            c->setColour (juce::ComboBox::arrowColourId, juce::Colours::white);
            addAndMakeVisible (c);
            
            l->setText (name, juce::dontSendNotification);
            l->setColour (juce::Label::textColourId, juce::Colours::white);
            l->setJustificationType (juce::Justification::centred);
            l->setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::bold));
            addAndMakeVisible (l);
            
            comboAttachments.add (new juce::AudioProcessorValueTreeState::ComboBoxAttachment (p.getAPVTS(), paramId, *c));
            comboMap.push_back ({ c, l, paramId });
        };

        setupSlider ("patch_program_number", "Prog Num");
        setupSlider ("patch_rx_tx_channel", "Rx/Tx Ch");
        setupCombo  ("patch_remain", "Remain", { "OFF", "ON" });
        setupSlider ("master_tune", "Master Tune");
        setupSlider ("master_key_shift", "Key Shift");
        setupSlider ("master_level", "Master Level");
        setupCombo  ("system_clock_source", "Clk Src", { "PATCH", "SYSTEM", "MIDI", "USB" });
        setupSlider ("system_tempo", "Tempo");
        setupCombo  ("receive_program_change", "Rx PC", { "OFF", "ON" });
        setupCombo  ("receive_bank_select", "Rx Bank", { "OFF", "ON" });
        setupCombo  ("transmit_edit_data", "Tx Edit", { "OFF", "ON" });
        setupCombo  ("soft_through", "Soft Thru", { "OFF", "ON" });
        setupCombo  ("midi_usb_thru", "USB Thru", { "OFF", "ON" });
        setupCombo  ("transmit_program_change", "Tx PC", { "OFF", "ON" });
        setupCombo  ("transmit_bank_select", "Tx Bank", { "OFF", "ON" });
        setupCombo  ("transmit_active_sensing", "Tx ActSens", { "OFF", "ON" });
        setupSlider ("transpose_value", "Transpose");
        setupCombo  ("local_switch", "Local Sw", { "OFF", "ON" });
        setupCombo  ("remote_keyboard_switch", "Rem KBD", { "OFF", "ON" });
        setupCombo  ("pedal_polarity", "Pedal Pol", { "STD", "REV" });
        setupCombo  ("pedal_assign", "Pedal Asgn", { "HOLD", "MOD", "VOL", "BAL", "PAN", "EXP", "START", "ARP" });
        setupSlider ("d_beam_sens", "DBeam Sens");
        setupCombo  ("recorder_sync_output", "Rec Sync", { "OFF", "ON" });
        setupCombo  ("recorder_metronome_mode", "Metro Mode", { "OFF", "REC", "REC&PLY", "ALW" });
        setupSlider ("recorder_metronome_level", "Metro Lvl");

        // --- Standalone Plugin Configuration Controls ---
        int currentW = editor.getWidth();
        int initialSizeId = 6; // Custom
        if (currentW == 960) initialSizeId = 1;
        else if (currentW == 1280) initialSizeId = 2;
        else if (currentW == 1600) initialSizeId = 3;
        else if (currentW == 1920) initialSizeId = 4;
        else if (currentW == 2560) initialSizeId = 5;

        guiSizeLabel.setText ("GUI Size", juce::dontSendNotification);
        guiSizeLabel.setColour (juce::Label::textColourId, juce::Colours::white);
        guiSizeLabel.setJustificationType (juce::Justification::centred);
        guiSizeLabel.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::bold));
        addAndMakeVisible (guiSizeLabel);

        guiSizeCombo.addItemList ({ "75%", "100%", "125%", "150%", "200%", "Custom" }, 1);
        guiSizeCombo.setSelectedId (initialSizeId, juce::dontSendNotification);
        guiSizeCombo.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2e333d));
        guiSizeCombo.setColour (juce::ComboBox::textColourId, juce::Colours::white);
        guiSizeCombo.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff555555));
        guiSizeCombo.setColour (juce::ComboBox::arrowColourId, juce::Colours::white);
        guiSizeCombo.onChange = [&editor, this] {
            int id = guiSizeCombo.getSelectedId();
            if (id == 1) { editor.setSize (960, 690); GlobalSettings::setGuiSize (960, 690); }
            else if (id == 2) { editor.setSize (1280, 920); GlobalSettings::setGuiSize (1280, 920); }
            else if (id == 3) { editor.setSize (1600, 1150); GlobalSettings::setGuiSize (1600, 1150); }
            else if (id == 4) { editor.setSize (1920, 1380); GlobalSettings::setGuiSize (1920, 1380); }
            else if (id == 5) { editor.setSize (2560, 1840); GlobalSettings::setGuiSize (2560, 1840); }
        };
        addAndMakeVisible (guiSizeCombo);

        int currentEditorMode = GlobalSettings::getEditorMode();
        editorModeLabel.setText ("Editor Mode", juce::dontSendNotification);
        editorModeLabel.setColour (juce::Label::textColourId, juce::Colours::white);
        editorModeLabel.setJustificationType (juce::Justification::centred);
        editorModeLabel.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::bold));
        addAndMakeVisible (editorModeLabel);

        editorModeCombo.addItemList ({ "USB MIDI + Audio", "USB MIDI Only", "DIN MIDI" }, 1);
        editorModeCombo.setSelectedId (currentEditorMode + 1, juce::dontSendNotification);
        editorModeCombo.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2e333d));
        editorModeCombo.setColour (juce::ComboBox::textColourId, juce::Colours::white);
        editorModeCombo.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff555555));
        editorModeCombo.setColour (juce::ComboBox::arrowColourId, juce::Colours::white);
        editorModeCombo.onChange = [&p, this] {
            int id = editorModeCombo.getSelectedId();
            int mode = id - 1; // 0 = MIDI + Audio, 1 = MIDI only, 2 = DIN MIDI
            GlobalSettings::setEditorMode (mode);
            updateDinMidiVisibility (mode == 2);
            p.reinitializeHardwareConnection();
            resized();
        };
        addAndMakeVisible (editorModeCombo);

        // --- DIN MIDI Port Controls ---
        midiInLabel.setText ("MIDI In Port", juce::dontSendNotification);
        midiInLabel.setColour (juce::Label::textColourId, juce::Colours::white);
        midiInLabel.setJustificationType (juce::Justification::centred);
        midiInLabel.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::bold));
        addChildComponent (midiInLabel);

        midiInCombo.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2e333d));
        midiInCombo.setColour (juce::ComboBox::textColourId, juce::Colours::white);
        midiInCombo.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff555555));
        midiInCombo.setColour (juce::ComboBox::arrowColourId, juce::Colours::white);
        addChildComponent (midiInCombo);

        midiOutLabel.setText ("MIDI Out Port", juce::dontSendNotification);
        midiOutLabel.setColour (juce::Label::textColourId, juce::Colours::white);
        midiOutLabel.setJustificationType (juce::Justification::centred);
        midiOutLabel.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::bold));
        addChildComponent (midiOutLabel);

        midiOutCombo.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2e333d));
        midiOutCombo.setColour (juce::ComboBox::textColourId, juce::Colours::white);
        midiOutCombo.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff555555));
        midiOutCombo.setColour (juce::ComboBox::arrowColourId, juce::Colours::white);
        addChildComponent (midiOutCombo);

        populateMidiDeviceCombos (p);
        updateDinMidiVisibility (currentEditorMode == 2);

        setSize (680, 440);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff363c45));
        g.setColour (juce::Colour (0xff4a525e));
        g.drawRect (getLocalBounds(), 2);

        // Subtle divider line separating synth system parameters and plugin settings
        g.setColour (juce::Colour (0xff282c33));
        g.drawLine (10.0f, 355.0f, static_cast<float> (getWidth() - 10), 355.0f, 1.5f);
    }

    void resized() override
    {
        int cols = 7;
        int cellW = getWidth() / cols;
        int cellH = 350 / 4; // Top 4 rows for synth parameters
        
        int i = 0;
        for (auto& item : sliderMap)
        {
            int r = i / cols;
            int c = i % cols;
            auto box = juce::Rectangle<int> (c * cellW, r * cellH, cellW, cellH).reduced (4);
            item.label->setBounds (box.removeFromTop (18));
            item.slider->setBounds (box.withSizeKeepingCentre (54, 66));
            i++;
        }
        for (auto& item : comboMap)
        {
            int r = i / cols;
            int c = i % cols;
            auto box = juce::Rectangle<int> (c * cellW, r * cellH, cellW, cellH).reduced (4);
            item.label->setBounds (box.removeFromTop (18));
            item.combo->setBounds (box.withSizeKeepingCentre (box.getWidth() - 8, 24));
            i++;
        }

        // Bottom section: Plugin & MIDI Settings (Y = 365)
        int bottomY = 365;
        guiSizeLabel.setBounds (10, bottomY, 100, 18);
        guiSizeCombo.setBounds (10, bottomY + 20, 100, 24);

        editorModeLabel.setBounds (120, bottomY, 150, 18);
        editorModeCombo.setBounds (120, bottomY + 20, 150, 24);

        if (midiInCombo.isVisible())
        {
            midiInLabel.setBounds (280, bottomY, 190, 18);
            midiInCombo.setBounds (280, bottomY + 20, 190, 24);

            midiOutLabel.setBounds (478, bottomY, 190, 18);
            midiOutCombo.setBounds (478, bottomY + 20, 190, 24);
        }
    }

private:
    void populateMidiDeviceCombos (SH201AudioProcessor& p)
    {
        availableInputs = juce::MidiInput::getAvailableDevices();
        midiInCombo.clear (juce::dontSendNotification);
        midiInCombo.addItem ("(None / Disabled)", 1);
        int selectedInId = 1;
        juce::String savedInId = GlobalSettings::getDinMidiIn();
        juce::String savedInName = GlobalSettings::getDinMidiInName();

        for (int i = 0; i < availableInputs.size(); ++i)
        {
            midiInCombo.addItem (availableInputs[i].name, i + 2);
            if (savedInId.isNotEmpty() && availableInputs[i].identifier == savedInId)
                selectedInId = i + 2;
            else if (savedInName.isNotEmpty() && availableInputs[i].name == savedInName && selectedInId == 1)
                selectedInId = i + 2;
        }
        midiInCombo.setSelectedId (selectedInId, juce::dontSendNotification);
        midiInCombo.onChange = [&p, this] {
            int id = midiInCombo.getSelectedId();
            if (id <= 1)
            {
                GlobalSettings::setDinMidiIn ("", "");
            }
            else
            {
                int idx = id - 2;
                if (idx >= 0 && idx < availableInputs.size())
                    GlobalSettings::setDinMidiIn (availableInputs[idx].identifier, availableInputs[idx].name);
            }
            p.reinitializeHardwareConnection();
        };

        availableOutputs = juce::MidiOutput::getAvailableDevices();
        midiOutCombo.clear (juce::dontSendNotification);
        midiOutCombo.addItem ("(None / Disabled)", 1);
        int selectedOutId = 1;
        juce::String savedOutId = GlobalSettings::getDinMidiOut();
        juce::String savedOutName = GlobalSettings::getDinMidiOutName();

        for (int i = 0; i < availableOutputs.size(); ++i)
        {
            midiOutCombo.addItem (availableOutputs[i].name, i + 2);
            if (savedOutId.isNotEmpty() && availableOutputs[i].identifier == savedOutId)
                selectedOutId = i + 2;
            else if (savedOutName.isNotEmpty() && availableOutputs[i].name == savedOutName && selectedOutId == 1)
                selectedOutId = i + 2;
        }
        midiOutCombo.setSelectedId (selectedOutId, juce::dontSendNotification);
        midiOutCombo.onChange = [&p, this] {
            int id = midiOutCombo.getSelectedId();
            if (id <= 1)
            {
                GlobalSettings::setDinMidiOut ("", "");
            }
            else
            {
                int idx = id - 2;
                if (idx >= 0 && idx < availableOutputs.size())
                    GlobalSettings::setDinMidiOut (availableOutputs[idx].identifier, availableOutputs[idx].name);
            }
            p.reinitializeHardwareConnection();
        };
    }

    void updateDinMidiVisibility (bool isDin)
    {
        midiInLabel.setVisible (isDin);
        midiInCombo.setVisible (isDin);
        midiOutLabel.setVisible (isDin);
        midiOutCombo.setVisible (isDin);
    }

    struct SliderItem { juce::Slider* slider; juce::Label* label; juce::String id; };
    struct ComboItem { juce::ComboBox* combo; juce::Label* label; juce::String id; };
    std::vector<SliderItem> sliderMap;
    std::vector<ComboItem> comboMap;
    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachments;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachments;

    juce::Label guiSizeLabel;
    juce::ComboBox guiSizeCombo;

    juce::Label editorModeLabel;
    juce::ComboBox editorModeCombo;

    juce::Label midiInLabel;
    juce::ComboBox midiInCombo;
    juce::Array<juce::MidiDeviceInfo> availableInputs;

    juce::Label midiOutLabel;
    juce::ComboBox midiOutCombo;
    juce::Array<juce::MidiDeviceInfo> availableOutputs;
};

class AdvancedPatchCommonComponent : public juce::Component
{
public:
    AdvancedPatchCommonComponent (SH201AudioProcessor& p, SH201LookAndFeel& lf)
    {
        auto setupSlider = [this, &p, &lf] (const juce::String& paramId, const juce::String& name)
        {
            auto* s = sliders.add (new juce::Slider());
            auto* l = labels.add (new juce::Label());
            s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s->setLookAndFeel (&lf);
            s->getProperties().set ("isVectorKnob", true);
            s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 54, 18);
            s->setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
            s->setColour (juce::Slider::thumbColourId, juce::Colour (0xff8893a0));
            s->setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffffffff));
            s->setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff999999));
            addAndMakeVisible (s);
            
            l->setText (name, juce::dontSendNotification);
            l->setColour (juce::Label::textColourId, juce::Colours::white);
            l->setJustificationType (juce::Justification::centred);
            l->setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::bold));
            addAndMakeVisible (l);
            
            sliderAttachments.add (new juce::AudioProcessorValueTreeState::SliderAttachment (p.getAPVTS(), paramId, *s));
            sliderMap.push_back ({ s, l, paramId });
        };

        auto setupCombo = [this, &p] (const juce::String& paramId, const juce::String& name, const juce::StringArray& items)
        {
            auto* c = combos.add (new juce::ComboBox());
            auto* l = labels.add (new juce::Label());
            c->addItemList (items, 1);
            c->setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2e333d));
            c->setColour (juce::ComboBox::textColourId, juce::Colours::white);
            c->setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff555555));
            c->setColour (juce::ComboBox::arrowColourId, juce::Colours::white);
            addAndMakeVisible (c);
            
            l->setText (name, juce::dontSendNotification);
            l->setColour (juce::Label::textColourId, juce::Colours::white);
            l->setJustificationType (juce::Justification::centred);
            l->setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::bold));
            addAndMakeVisible (l);
            
            comboAttachments.add (new juce::AudioProcessorValueTreeState::ComboBoxAttachment (p.getAPVTS(), paramId, *c));
            comboMap.push_back ({ c, l, paramId });
        };

        setupSlider ("common_patch_level", "Patch Lvl");
        setupSlider ("common_patch_tempo", "Tempo");
        setupCombo  ("common_keyboard_part", "KBD Part", { "UPPER", "LOWER" });
        setupCombo  ("common_split_arpeggio", "Split Arp", { "UPPER", "LOWER", "BOTH" });
        setupCombo  ("common_mod_destination", "Mod Dest", { "UPPER", "LOWER", "BOTH" });
        setupCombo  ("common_dbeam_destination", "DBeam Dest", { "UPPER", "LOWER", "BOTH" });
        setupCombo  ("common_pitch_bend_destination", "Bend Dest", { "UPPER", "LOWER", "BOTH" });
        setupCombo  ("common_expression_destination", "Expr Dest", { "UPPER", "LOWER", "BOTH" });
        setupCombo  ("common_active_expression_switch", "ActExpr Sw", { "OFF", "ON" });
        setupCombo  ("common_arpeggio_switch", "Arp Sw", { "OFF", "ON" });
        setupCombo  ("common_arpeggio_hold", "Arp Hold", { "OFF", "ON" });
        setupCombo  ("common_modulation_assign", "Mod Asgn", { "OSC1&2", "OSC1", "OSC2", "PW1", "PW2", "FILTER", "AMP", "AUD-FILT" });
        setupCombo  ("common_dbeam_assign", "DBeam Asgn", { "O1-PITCH", "O1-DETUNE", "O1-PW", "O2-PITCH", "O2-DETUNE", "O2-PW", "MIX-BAL", "FILT-CUT", "FILT-RES", "FILT-KEYFOL", "AMP-LEVEL", "AUD-CUT", "AUD-RES", "P-ENV-A", "P-ENV-D", "O1-P-DEP", "O2-P-DEP", "LFO1-RATE", "LFO1-DEP1", "LFO1-DEP2", "LFO2-RATE", "LFO2-DEP1", "LFO2-DEP2", "F-ENV-A", "F-ENV-D", "F-ENV-S", "F-ENV-R", "F-ENV-DEP", "A-ENV-A", "A-ENV-D", "A-ENV-S", "A-ENV-R", "DLY-TIME", "DLY-DEP", "REV-TIME", "REV-DEP", "BENDER" });
        setupCombo  ("common_dbeam_polarity", "DBeam Pol", { "NORMAL", "REVERSE" });

        setSize (600, 300);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff363c45));
        g.setColour (juce::Colour (0xff4a525e));
        g.drawRect (getLocalBounds(), 2);
    }

    void resized() override
    {
        int cols = 5;
        int cellW = getWidth() / cols;
        int cellH = getHeight() / 3;
        
        int i = 0;
        for (auto& item : sliderMap)
        {
            int r = i / cols;
            int c = i % cols;
            auto box = juce::Rectangle<int> (c * cellW, r * cellH, cellW, cellH).reduced (4);
            item.label->setBounds (box.removeFromTop (18));
            item.slider->setBounds (box.withSizeKeepingCentre (54, 66));
            i++;
        }
        for (auto& item : comboMap)
        {
            int r = i / cols;
            int c = i % cols;
            auto box = juce::Rectangle<int> (c * cellW, r * cellH, cellW, cellH).reduced (4);
            item.label->setBounds (box.removeFromTop (18));
            item.combo->setBounds (box.withSizeKeepingCentre (box.getWidth() - 8, 24));
            i++;
        }
    }

private:
    struct SliderItem { juce::Slider* slider; juce::Label* label; juce::String id; };
    struct ComboItem { juce::ComboBox* combo; juce::Label* label; juce::String id; };
    std::vector<SliderItem> sliderMap;
    std::vector<ComboItem> comboMap;
    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachments;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachments;
};


class AdvancedEffectsComponent : public juce::Component
{
public:
    AdvancedEffectsComponent (SH201AudioProcessor& p, SH201LookAndFeel& lf)
    {
        auto setupSlider = [this, &p, &lf] (const juce::String& paramId, const juce::String& name)
        {
            auto* s = sliders.add (new juce::Slider());
            auto* l = labels.add (new juce::Label());
            s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s->setLookAndFeel (&lf);
            s->getProperties().set ("isVectorKnob", true);
            s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 54, 18);
            s->setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
            s->setColour (juce::Slider::thumbColourId, juce::Colour (0xff8893a0));
            s->setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffffffff));
            s->setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff999999));
            addAndMakeVisible (s);
            
            l->setText (name, juce::dontSendNotification);
            l->setColour (juce::Label::textColourId, juce::Colours::white);
            l->setJustificationType (juce::Justification::centred);
            l->setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::bold));
            addAndMakeVisible (l);
            
            sliderAttachments.add (new juce::AudioProcessorValueTreeState::SliderAttachment (p.getAPVTS(), paramId, *s));
            items.push_back ({ s, l, true });
        };

        auto setupCombo = [this, &p, &lf] (const juce::String& paramId, const juce::String& name, const juce::StringArray& choices)
        {
            auto* c = combos.add (new juce::ComboBox());
            auto* l = labels.add (new juce::Label());
            c->setLookAndFeel (&lf);
            c->addItemList (choices, 1);
            c->setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2e333d));
            c->setColour (juce::ComboBox::textColourId, juce::Colours::white);
            c->setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff555555));
            c->setColour (juce::ComboBox::arrowColourId, juce::Colours::white);
            addAndMakeVisible (c);
            
            l->setText (name, juce::dontSendNotification);
            l->setColour (juce::Label::textColourId, juce::Colours::white);
            l->setJustificationType (juce::Justification::centred);
            l->setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::bold));
            addAndMakeVisible (l);
            
            comboAttachments.add (new juce::AudioProcessorValueTreeState::ComboBoxAttachment (p.getAPVTS(), paramId, *c));
            items.push_back ({ c, l, false });
        };

        setupSlider ("patch_delay_time", "Dly Time");
        setupSlider ("patch_delay_feedback", "Dly Fdbk");
        setupCombo  ("patch_delay_hf_damp", "Dly HF Damp", { "200", "250", "315", "400", "500", "630", "800", "1000", "1250", "1600", "2000", "2500", "3150", "4000", "5000", "6300", "8000", "BYPASS" });
        setupSlider ("patch_delay_mod_rate", "Dly Mod Rate");
        setupSlider ("patch_delay_mod_depth", "Dly Mod Dpth");

        setupSlider ("patch_reverb_time", "Rev Time");
        setupSlider ("patch_reverb_pre_delay", "Rev Pre Dly");
        setupSlider ("patch_reverb_size", "Rev Size");
        setupCombo  ("patch_reverb_high_cut", "Rev High Cut", { "160", "200", "250", "320", "400", "500", "640", "800", "1000", "1250", "1600", "2000", "2500", "3200", "4000", "5000", "6400", "8000", "10K", "12.5K", "BYP" });
        setupSlider ("patch_reverb_density", "Rev Density");
        setupSlider ("patch_reverb_diffusion", "Rev Diffuse");
        setupCombo  ("patch_reverb_lf_damp_freq", "Rev LF Freq", { "50", "64", "80", "100", "125", "160", "200", "250", "320", "400", "500", "640", "800", "1000", "1250", "1600", "2000", "2500", "3200", "4000" });
        setupSlider ("patch_reverb_lf_damp_gain", "Rev LF Gain");
        setupCombo  ("patch_reverb_hf_damp_freq", "Rev HF Freq", { "4000", "5000", "6400", "8000", "10000", "12500" });
        setupSlider ("patch_reverb_hf_damp_gain", "Rev HF Gain");

        setSize (600, 300);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff363c45));
        g.setColour (juce::Colour (0xff4a525e));
        g.drawRect (getLocalBounds(), 2);
    }

    void resized() override
    {
        int cols = 5;
        int cellW = getWidth() / cols;
        int cellH = getHeight() / 3;
        
        for (int i = 0; i < (int) items.size(); ++i)
        {
            int r = i / cols;
            int c = i % cols;
            auto box = juce::Rectangle<int> (c * cellW, r * cellH, cellW, cellH).reduced (4);
            items[i].label->setBounds (box.removeFromTop (18));
            if (items[i].isSlider)
                items[i].control->setBounds (box.withSizeKeepingCentre (54, 66));
            else
                items[i].control->setBounds (box.withSizeKeepingCentre (box.getWidth() - 8, 24));
        }
    }

private:
    struct ControlItem { juce::Component* control; juce::Label* label; bool isSlider; };
    std::vector<ControlItem> items;
    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachments;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachments;
};

SH201AudioProcessorEditor::SH201AudioProcessorEditor (SH201AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    brushedMetalTexture = createBrushedMetalImage (512, 512);

    // --- Header Title ---
    titleLabel.setText ("ROLAND SH-201", juce::dontSendNotification);
    titleLabel.setFont (juce::Font ("Segoe UI", 22.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff111111));
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("EDITOR VST v1.2.1", juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::bold));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8a93a0));
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (subtitleLabel);

    // --- Status Label ---
    statusLabel.setFont (juce::Font ("Segoe UI", 12.0f, juce::Font::bold));
    statusLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (statusLabel);

    // --- Header Buttons ---
    styleButton (pullPatchButton, juce::Colour (0xff555555));
    pullPatchButton.setButtonText ("PULL SYNTH");
    pullPatchButton.onClick = [this] {
        audioProcessor.isSyncingFromHardware.store (true);
        audioProcessor.requestPatchMemoryFromHardware();
    };

    styleButton (reconnectButton, juce::Colour (0xff666666));
    reconnectButton.setButtonText ("RECONNECT");
    reconnectButton.onClick = [this] {
        audioProcessor.reinitializeHardwareConnection();
        timerCallback();
    };

    styleButton (systemSettingsButton, juce::Colour (0xff444466));
    systemSettingsButton.setButtonText ("SYSTEM SETTINGS");
    systemSettingsButton.onClick = [this] {
        auto* comp = new SystemSettingsComponent (*this, audioProcessor, customLookAndFeel);
        juce::CallOutBox::launchAsynchronously (std::unique_ptr<juce::Component>(comp), systemSettingsButton.getScreenBounds(), nullptr);
    };

    styleButton (patchBrowserButton, juce::Colour (0xff3d5a80));
    patchBrowserButton.setButtonText ("PATCH BROWSER");
    patchBrowserButton.setClickingTogglesState (true);
    patchBrowserButton.onClick = [this] {
        currentViewMode = patchBrowserButton.getToggleState() ? ViewMode::Browser : ViewMode::Editor;
        updateViewModeVisibility();
    };
    addAndMakeVisible (patchBrowserButton);

    patchBrowser = std::make_unique<PatchBrowserComponent> (audioProcessor, customLookAndFeel, [this] {
        patchBrowserButton.setToggleState (false, juce::sendNotification);
    });
    addChildComponent (patchBrowser.get());

    // --- Footer Label ---
    lastReceivedLabel.setFont (juce::Font ("Segoe UI", 12.0f, juce::Font::bold));
    lastReceivedLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    lastReceivedLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (lastReceivedLabel);

    // Helper to add all 48 Tone controls for a given prefix ("upper_" or "lower_")
    auto addToneControls = [this] (const juce::String& prefix)
    {
        auto addToggleButton = [this](const juce::String& paramId, const juce::String& text)
        {
            auto* btn = toggleButtons.add (new juce::TextButton());
            auto* glow = toggleGlows.add (new juce::GlowEffect());
            
            styleButton (*btn, juce::Colour (0xff4a525e));
            btn->setButtonText (text);
            btn->setClickingTogglesState (true);
            btn->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe74c3c));
            btn->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
            
            glow->setGlowProperties (3.0f, juce::Colour(0xffe74c3c).withAlpha(0.6f), juce::Point<int>(0, 0));
            
            btn->onClick = [btn, glow] {
                if (btn->getToggleState())
                    btn->setComponentEffect (glow);
                else
                    btn->setComponentEffect (nullptr);
            };
            addAndMakeVisible (btn);
            
            toggleAttachments.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (audioProcessor.getAPVTS(), paramId, *btn));
            buttonMap[paramId] = btn;
            
            if (btn->getToggleState())
                btn->setComponentEffect (glow);
        };

        // --- OSCILLATOR 1 ---
        addComboBox     (prefix + "osc1_wave", "WAVE", { "Saw", "Square", "PW-Sqr", "Tri", "Sine", "Noise", "Feedbk", "SuperSaw", "Ext" });
        addToggleButton (prefix + "osc1_pitch_wide", "WIDE");
        addRotarySlider (prefix + "osc1_pitch", "PITCH");
        addRotarySlider (prefix + "osc1_fine_tune", "DETUNE");
        addRotarySlider (prefix + "osc1_pulse_width", "PW/FEEDBK");
        addRotarySlider (prefix + "osc1_pitch_env_depth", "P.ENV");

        // Hook Osc 1 WIDE button to dynamically update pitch slider range
        {
            auto* wideBtn = buttonMap[prefix + "osc1_pitch_wide"];
            auto pitchId = prefix + "osc1_pitch";
            auto existingOnClick = wideBtn->onClick;
            wideBtn->onClick = [this, wideBtn, pitchId, existingOnClick] {
                if (existingOnClick) existingOnClick();
                updatePitchSliderRange (pitchId, wideBtn->getToggleState());
            };
        }

        // --- OSCILLATOR 2 ---
        addComboBox     (prefix + "osc2_wave", "WAVE", { "Saw", "Square", "PW-Sqr", "Tri", "Sine", "Noise", "Feedbk", "SuperSaw", "Ext" });
        addToggleButton (prefix + "osc2_pitch_wide", "WIDE");
        addRotarySlider (prefix + "osc2_pitch", "PITCH");
        addRotarySlider (prefix + "osc2_detune", "DETUNE");
        addRotarySlider (prefix + "osc2_pulse_width", "PW/FEEDBK");
        addRotarySlider (prefix + "osc2_pitch_env_depth", "P.ENV");

        // Hook Osc 2 WIDE button to dynamically update pitch slider range
        {
            auto* wideBtn = buttonMap[prefix + "osc2_pitch_wide"];
            auto pitchId = prefix + "osc2_pitch";
            auto existingOnClick = wideBtn->onClick;
            wideBtn->onClick = [this, wideBtn, pitchId, existingOnClick] {
                if (existingOnClick) existingOnClick();
                updatePitchSliderRange (pitchId, wideBtn->getToggleState());
            };
        }

        // --- MIX / MOD ---
        addComboBox     (prefix + "mix_mod_type", "TYPE", { "MIX", "SYNC", "RING" });
        addRotarySlider (prefix + "mix_mod_balance", "OSC BAL");
        addComboBox     (prefix + "mix_mod_low_freq", "LOW FREQ", { "FLAT", "BOOST", "CUT" });

        // --- PITCH ENVELOPE ---
        addLinearSlider (prefix + "pitch_env_attack", "ATTACK");
        addLinearSlider (prefix + "pitch_env_decay", "DECAY");

        // --- FILTER ---
        addComboBox     (prefix + "filter_type", "TYPE", { "BYPASS", "LPF", "HPF", "BPF" });
        addComboBox     (prefix + "filter_slope", "SLOPE", { "-12dB", "-24dB" });
        addRotarySlider (prefix + "filter_cutoff", "CUTOFF");
        addRotarySlider (prefix + "filter_resonance", "RESO");
        addRotarySlider (prefix + "filter_keyfollow", "KEYFLW");
        addRotarySlider (prefix + "filter_velocity_sens", "VEL.SNS");

        // --- FILTER ENVELOPE ---
        addLinearSlider (prefix + "filter_attack", "A");
        addLinearSlider (prefix + "filter_decay", "D");
        addLinearSlider (prefix + "filter_sustain", "S");
        addLinearSlider (prefix + "filter_release", "R");
        addLinearSlider (prefix + "filter_env_depth", "DEPTH");

        // --- AMPLIFIER & OVERDRIVE ---
        addRotarySlider (prefix + "amp_level", "LEVEL");
        addRotarySlider (prefix + "amp_pan", "PAN");
        addRotarySlider (prefix + "amp_velocity_sens", "VEL.SNS");
        addToggleButton (prefix + "amp_overdrive_switch", "OVERDRIVE");
        addRotarySlider (prefix + "overdrive_level", "DRIVE");

        // --- AMP ENVELOPE ---
        addLinearSlider (prefix + "amp_attack", "A");
        addLinearSlider (prefix + "amp_decay", "D");
        addLinearSlider (prefix + "amp_sustain", "S");
        addLinearSlider (prefix + "amp_release", "R");

        // --- LFO 1 ---
        addComboBox     (prefix + "lfo1_shape", "SHAPE", { "TRI", "SIN", "SAW", "SQR", "TRP", "S&H", "RND" });
        addRotarySlider (prefix + "lfo1_rate", "RATE");
        addComboBox     (prefix + "lfo1_tempo_note", "NOTE", { "16", "12", "8", "4", "2", "1", "3/4", "2/3", "1/2", "3/8", "1/3", "1/4", "3/16", "1/6", "1/8", "3/32", "1/12", "1/16", "1/24", "1/32" });
        addToggleButton (prefix + "lfo1_tempo_sync", "SYNC");
        addToggleButton (prefix + "lfo1_key_trigger", "KEYTRIG");
        addRotarySlider (prefix + "lfo1_depth", "DEPTH 1");
        addComboBox     (prefix + "lfo1_dest1", "DEST 1", { "PITCH1", "PW1", "FILTER", "A-FILT" });
        addRotarySlider (prefix + "lfo1_depth2", "DEPTH 2");
        addComboBox     (prefix + "lfo1_dest2", "DEST 2", { "PITCH2", "PW2", "AMP" });
        addLinearSlider (prefix + "lfo1_fade_time", "FADE");

        // --- LFO 2 ---
        addComboBox     (prefix + "lfo2_shape", "SHAPE", { "TRI", "SIN", "SAW", "SQR", "TRP", "S&H", "RND" });
        addRotarySlider (prefix + "lfo2_rate", "RATE");
        addComboBox     (prefix + "lfo2_tempo_note", "NOTE", { "16", "12", "8", "4", "2", "1", "3/4", "2/3", "1/2", "3/8", "1/3", "1/4", "3/16", "1/6", "1/8", "3/32", "1/12", "1/16", "1/24", "1/32" });
        addToggleButton (prefix + "lfo2_tempo_sync", "SYNC");
        addToggleButton (prefix + "lfo2_key_trigger", "KEYTRIG");
        addRotarySlider (prefix + "lfo2_depth", "DEPTH 1");
        addComboBox     (prefix + "lfo2_dest1", "DEST 1", { "PITCH1", "PW1", "FILTER", "A-FILT" });
        addRotarySlider (prefix + "lfo2_depth2", "DEPTH 2");
        addComboBox     (prefix + "lfo2_dest2", "DEST 2", { "PITCH2", "PW2", "AMP" });
        addLinearSlider (prefix + "lfo2_fade_time", "FADE");
    };

    addToneControls ("upper_");
    addToneControls ("lower_");

    // --- STATIC COMMON / MASTER PARAMETERS ---
    addComboBox     ("common_keyboard_mode", "KEY MODE", { "SINGLE", "DUAL", "SPLIT" });
    addRotarySlider ("common_tone_balance", "TONE BAL");
    addRotarySlider ("common_split_point", "SPLIT PT");

    addComboBox     ("common_mono_solo_select", "VOICE", { "POLY", "SOLO+L", "SOLO" });
    addComboBox     ("common_portamento_switch", "PORTA", { "OFF", "ON" });
    addRotarySlider ("common_portamento_time", "TIME");
    addRotarySlider ("common_pitch_bend_range", "BEND");
    addRotarySlider ("common_octave_shift", "OCTAVE");

    styleButton (advancedPatchCommonButton, juce::Colour (0xff333333));
    advancedPatchCommonButton.setButtonText ("ADVANCED");
    advancedPatchCommonButton.onClick = [this] {
        auto* comp = new AdvancedPatchCommonComponent (audioProcessor, customLookAndFeel);
        juce::CallOutBox::launchAsynchronously (std::unique_ptr<juce::Component>(comp), advancedPatchCommonButton.getScreenBounds(), nullptr);
    };

    styleButton (advancedEffectsButton, juce::Colour (0xff333333));
    advancedEffectsButton.setButtonText ("ADVANCED");
    advancedEffectsButton.onClick = [this] {
        auto* comp = new AdvancedEffectsComponent (audioProcessor, customLookAndFeel);
        juce::CallOutBox::launchAsynchronously (std::unique_ptr<juce::Component>(comp), advancedEffectsButton.getScreenBounds(), nullptr);
    };

    auto initFxButton = [this](juce::TextButton& btn, std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& attachment, const juce::String& paramId, const juce::String& text, juce::GlowEffect& glow)
    {
        styleButton (btn, juce::Colour (0xff4a525e));
        btn.setButtonText (text);
        btn.setClickingTogglesState (true);
        btn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe74c3c));
        btn.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        
        glow.setGlowProperties (3.0f, juce::Colour(0xffe74c3c).withAlpha(0.6f), juce::Point<int>(0, 0));
        
        btn.onClick = [&btn, &glow] {
            if (btn.getToggleState())
                btn.setComponentEffect (&glow);
            else
                btn.setComponentEffect (nullptr);
        };
        addAndMakeVisible (btn);
        
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.getAPVTS(), paramId, btn);
        
        // Initial state
        if (btn.getToggleState())
            btn.setComponentEffect (&glow);
    };

    initFxButton (delaySwitchButton, delaySwitchAttachment, "common_delay_switch", "DLY SW", delayGlow);
    addRotarySlider ("common_delay_level", "DELAY");
    initFxButton (reverbSwitchButton, reverbSwitchAttachment, "common_reverb_switch", "REV SW", reverbGlow);
    addRotarySlider ("common_reverb_level", "REVERB");

    // --- STEREO MODE (SH-201 EDITOR) ---
    addRotarySlider ("common_stereo_width", "WIDTH");
    addComboBox     ("common_stereo_link_mode", "PARAM LINK", { "LINK ALL", "EXCLUDE OSC" });

    styleButton (stereoModeButton, juce::Colour (0xff4a525e));
    stereoModeButton.setButtonText ("STEREO MODE");
    stereoModeButton.setClickingTogglesState (true);
    stereoModeButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe74c3c));
    stereoModeButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    addAndMakeVisible (stereoModeButton);
    stereoModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.getAPVTS(), "common_stereo_mode", stereoModeButton);

    // --- TONE PAGE TOGGLE BUTTONS ---
    auto setupLinkButton = [this, &p] (const juce::String& paramId)
    {
        auto* b = linkButtons.add (new juce::TextButton());
        b->setButtonText ("LINK");
        b->setClickingTogglesState (true);
        b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff333333));
        b->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe74c3c));
        b->setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        b->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        addAndMakeVisible (b);
        linkAttachments.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (p.getAPVTS(), paramId, *b));
        return b;
    };

    setupLinkButton ("link_osc1");
    setupLinkButton ("link_osc2");
    setupLinkButton ("link_mix_mod");
    setupLinkButton ("link_pitch_env");
    setupLinkButton ("link_filter");
    setupLinkButton ("link_filter_env");
    setupLinkButton ("link_amp_env");
    setupLinkButton ("link_amp");
    setupLinkButton ("link_lfo1");
    setupLinkButton ("link_lfo2");

    styleButton (upperPageButton, juce::Colour (0xffe74c3c));
    upperPageButton.setButtonText ("UPPER TONE PAGE");
    upperPageButton.onClick = [this] {
        currentTonePage = TonePage::Upper;
        updatePageButtons();
        updateTonePageVisibility();
        audioProcessor.sendToneSelectToHardware (0);
        audioProcessor.setCurrentTonePage (0);
    };
    addAndMakeVisible (upperPageButton);

    styleButton (lowerPageButton, juce::Colour (0xff4a525e));
    lowerPageButton.setButtonText ("LOWER TONE PAGE");
    lowerPageButton.onClick = [this] {
        currentTonePage = TonePage::Lower;
        updatePageButtons();
        updateTonePageVisibility();
        audioProcessor.sendToneSelectToHardware (1);
        audioProcessor.setCurrentTonePage (1);
    };
    addAndMakeVisible (lowerPageButton);

    styleButton (initPatchButton, juce::Colour (0xff4a525e));
    initPatchButton.setButtonText ("INIT PATCH");
    initPatchButton.onClick = [this] {
        if (stereoModeButton.getToggleState())
            stereoModeButton.setToggleState (false, juce::sendNotification);
        if (auto* param = audioProcessor.getAPVTS().getParameter ("common_stereo_mode"))
            param->setValueNotifyingHost (0.0f);

        currentTonePage = TonePage::Upper;
        updatePageButtons();
        updateTonePageVisibility();
        audioProcessor.setCurrentTonePage (0);

        int channel = 0;
        if (auto* p = audioProcessor.getAPVTS().getParameter ("patch_rx_tx_channel"))
            channel = juce::jlimit (0, 15, static_cast<int> (std::round (p->getValue() * 15.0f)));

        audioProcessor.isSyncingFromHardware.store (true);

        // Send SysEx DT1 to 01 00 00 00 (System: Patch Bank Select MSB, LSB, PC)
        std::vector<uint8_t> patchSelectSysEx = {
            0xF0, 0x41, 0x10, 0x00, 0x00, 0x16, 0x12,
            0x01, 0x00, 0x00, 0x00,  // Address: 01 00 00 00
            0x57, 0x20, 0x1F,        // Data: MSB=87 (0x57), LSB=32 (0x20 -> PRESET), PC=31 (0x1F) -> Preset D-8
            0x69,                    // Checksum (128 - ((1 + 87 + 32 + 31) % 128)) % 128 = 105 (0x69)
            0xF7
        };
        audioProcessor.sendSysExToHardware (patchSelectSysEx);

        // Also send MIDI Bank Select & Program Change
        audioProcessor.sendMidiCCToHardware (static_cast<uint8_t> (channel), 0, 87);
        audioProcessor.sendMidiCCToHardware (static_cast<uint8_t> (channel), 32, 32); // 32 = 0x20 (PRESET)
        audioProcessor.sendMidiMessageToHardware (juce::MidiMessage::programChange (channel + 1, 31));

        if (auto* p = audioProcessor.getAPVTS().getParameter ("patch_bank_msb"))
            p->setValueNotifyingHost (p->convertTo0to1 (87.0f));
        if (auto* p = audioProcessor.getAPVTS().getParameter ("patch_bank_lsb"))
            p->setValueNotifyingHost (p->convertTo0to1 (32.0f));
        if (auto* p = audioProcessor.getAPVTS().getParameter ("patch_program_number"))
            p->setValueNotifyingHost (p->convertTo0to1 (31.0f));

        // After sending the patch change, wait for hardware to settle, then pull
        // the full patch back (like PULL SYNTH) so the UI reflects the init state.
        juce::Timer::callAfterDelay (300, [this]() {
            audioProcessor.sendToneSelectToHardware (0);
            juce::Timer::callAfterDelay (200, [this]() {
                audioProcessor.isSyncingFromHardware.store (true);
                audioProcessor.requestPatchMemoryFromHardware();

                // Safety timeout: ensure the flag clears even if the hardware
                // drops a SysEx response, so the UI never stays stuck on "SYNCING".
                juce::Timer::callAfterDelay (500, [this]() {
                    audioProcessor.isSyncingFromHardware.store (false);
                });
            });
        });
    };
    addAndMakeVisible (initPatchButton);

    setResizable (true, true);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (1280.0 / 920.0);
    setResizeLimits (640, 460, 2560, 1840);

    int w = 1280;
    int h = 920;
    GlobalSettings::getGuiSize (w, h);
    setSize (w, h);  // Must be AFTER all controls are created so resized() can find them

    // Force size asynchronously to override any initial resizing done by the DAW's host wrappers
    juce::MessageManager::callAsync ([this, w, h]() {
        if (getWidth() != w || getHeight() != h)
            setSize (w, h);
            
        juce::Timer::callAfterDelay (500, [this]() {
            editorIsReady = true;
        });
    });
    updatePageButtons();
    updateViewModeVisibility();
    updateAllPitchSliderRanges();

    startTimerHz (4);
    timerCallback();
}

SH201AudioProcessorEditor::~SH201AudioProcessorEditor()
{
    stopTimer();
}

void SH201AudioProcessorEditor::updatePageButtons()
{
    upperPageButton.setColour (juce::TextButton::buttonColourId,
                               currentTonePage == TonePage::Upper ? juce::Colour (0xffe74c3c) : juce::Colour (0xff4a525e));
    lowerPageButton.setColour (juce::TextButton::buttonColourId,
                               currentTonePage == TonePage::Lower ? juce::Colour (0xffe74c3c) : juce::Colour (0xff4a525e));
}

void SH201AudioProcessorEditor::updateViewModeVisibility()
{
    bool isBrowser = (currentViewMode == ViewMode::Browser);

    if (patchBrowser)
    {
        patchBrowser->setVisible (isBrowser);
        if (isBrowser)
        {
            patchBrowser->refreshBrowser();
            patchBrowser->toFront (true);
        }
    }

    upperPageButton.setVisible (! isBrowser);
    lowerPageButton.setVisible (! isBrowser);
    initPatchButton.setVisible (! isBrowser);
    patchBrowserButton.setVisible (! isBrowser);
    stereoModeButton.setVisible (! isBrowser);
    advancedPatchCommonButton.setVisible (! isBrowser);
    advancedEffectsButton.setVisible (! isBrowser);

    for (auto* btn : linkButtons)
        if (btn) btn->setVisible (! isBrowser);

    if (isBrowser)
    {
        for (auto& pair : sliderMap)
            if (pair.second != nullptr) pair.second->setVisible (false);
        for (auto& pair : comboMap)
            if (pair.second != nullptr) pair.second->setVisible (false);
        for (auto& pair : labelMap)
            if (pair.second != nullptr) pair.second->setVisible (false);
        for (auto& pair : buttonMap)
            if (pair.second != nullptr) pair.second->setVisible (false);
    }
    else
    {
        updateTonePageVisibility();
    }

    repaint();
}

void SH201AudioProcessorEditor::updateTonePageVisibility()
{
    if (currentViewMode == ViewMode::Browser)
        return;
    for (auto& pair : sliderMap)
    {
        if (pair.second != nullptr)
        {
            if (pair.first.startsWith ("upper_"))
                pair.second->setVisible (currentTonePage == TonePage::Upper);
            else if (pair.first.startsWith ("lower_"))
                pair.second->setVisible (currentTonePage == TonePage::Lower);
            else
                pair.second->setVisible (true);
        }
    }
    for (auto& pair : comboMap)
    {
        if (pair.second != nullptr)
        {
            if (pair.first.startsWith ("upper_"))
                pair.second->setVisible (currentTonePage == TonePage::Upper);
            else if (pair.first.startsWith ("lower_"))
                pair.second->setVisible (currentTonePage == TonePage::Lower);
            else
                pair.second->setVisible (true);
        }
    }
    for (auto& pair : labelMap)
    {
        if (pair.second != nullptr)
        {
            if (pair.first.startsWith ("upper_"))
                pair.second->setVisible (currentTonePage == TonePage::Upper);
            else if (pair.first.startsWith ("lower_"))
                pair.second->setVisible (currentTonePage == TonePage::Lower);
            else
                pair.second->setVisible (true);
        }
    }
    for (auto& pair : buttonMap)
    {
        if (pair.second != nullptr)
        {
            if (pair.first.startsWith ("upper_"))
                pair.second->setVisible (currentTonePage == TonePage::Upper);
            else if (pair.first.startsWith ("lower_"))
                pair.second->setVisible (currentTonePage == TonePage::Lower);
            else
                pair.second->setVisible (true);
        }
    }
    repaint();
}


juce::Slider* SH201AudioProcessorEditor::addRotarySlider (const juce::String& paramId, const juce::String& labelText)
{
    auto* s = sliders.add (new juce::Slider());
    auto* l = labels.add (new juce::Label());
    styleRotarySlider (*s, *l, paramId, labelText);
    sliderMap[paramId] = s;
    labelMap[paramId] = l;
    sliderAttachments.add (new juce::AudioProcessorValueTreeState::SliderAttachment (audioProcessor.getAPVTS(), paramId, *s));
    return s;
}

juce::Slider* SH201AudioProcessorEditor::addLinearSlider (const juce::String& paramId, const juce::String& labelText)
{
    auto* s = sliders.add (new juce::Slider());
    auto* l = labels.add (new juce::Label());
    styleLinearSlider (*s, *l, paramId, labelText);
    sliderMap[paramId] = s;
    labelMap[paramId] = l;
    sliderAttachments.add (new juce::AudioProcessorValueTreeState::SliderAttachment (audioProcessor.getAPVTS(), paramId, *s));
    return s;
}

juce::ComboBox* SH201AudioProcessorEditor::addComboBox (const juce::String& paramId, const juce::String& labelText, const juce::StringArray& items)
{
    auto* c = comboBoxes.add (new juce::ComboBox());
    auto* l = labels.add (new juce::Label());
    styleComboBox (*c, *l, paramId, labelText);
    c->addItemList (items, 1);
    comboMap[paramId] = c;
    labelMap[paramId] = l;
    comboAttachments.add (new juce::AudioProcessorValueTreeState::ComboBoxAttachment (audioProcessor.getAPVTS(), paramId, *c));
    return c;
}

void SH201AudioProcessorEditor::styleRotarySlider (juce::Slider& slider, juce::Label& label, const juce::String& paramId, const juce::String& name)
{
    bool isSilver = paramId.contains("osc1") || paramId.contains("osc2") || paramId.contains("mix_mod") ||
                    paramId.contains("filter_type") || paramId.contains("filter_slope") || 
                    paramId.contains("filter_cutoff") || paramId.contains("filter_resonance") || 
                    paramId.contains("filter_keyfollow") || paramId.contains("filter_velocity_sens") ||
                    paramId.contains("amp_level") || paramId.contains("amp_pan") || 
                    paramId.contains("amp_velocity_sens") || paramId.contains("amp_overdrive_switch") || 
                    paramId.contains("overdrive_level");

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setLookAndFeel (&customLookAndFeel);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 54, 18);
    slider.setColour (juce::Slider::thumbColourId, isSilver ? juce::Colour (0xff222222) : juce::Colour (0xff8893a0));
    slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffffffff));
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff999999));
    slider.setColour (juce::Slider::textBoxTextColourId, isSilver ? juce::Colours::black : juce::Colours::white);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setFont (juce::Font ("Segoe UI", 12.0f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, isSilver ? juce::Colours::black : juce::Colours::white);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);
}

void SH201AudioProcessorEditor::styleLinearSlider (juce::Slider& slider, juce::Label& label, const juce::String& paramId, const juce::String& name)
{
    // All linear sliders are in dark sections (Filter Env, Amp Env)
    bool isSilver = false;

    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setLookAndFeel (&customLookAndFeel);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 44, 18);
    slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xff8893a0));
    slider.setColour (juce::Slider::trackColourId, juce::Colour (0xffffffff));
    slider.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff222936));
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setFont (juce::Font ("Segoe UI", 12.0f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, isSilver ? juce::Colours::black : juce::Colours::white);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);
}

void SH201AudioProcessorEditor::styleComboBox (juce::ComboBox& combo, juce::Label& label, const juce::String& paramId, const juce::String& name)
{
    bool isSilver = paramId.contains("osc1") || paramId.contains("osc2") || paramId.contains("mix_mod") ||
                    paramId.contains("filter_type") || paramId.contains("filter_slope") || 
                    paramId.contains("amp_overdrive_switch");

    combo.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2e333d));
    combo.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    combo.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff4a525e));
    combo.setColour (juce::ComboBox::arrowColourId, juce::Colours::white);
    addAndMakeVisible (combo);

    label.setText (name, juce::dontSendNotification);
    label.setFont (juce::Font ("Segoe UI", 12.0f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, isSilver ? juce::Colours::black : juce::Colours::white);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);
}

void SH201AudioProcessorEditor::styleButton (juce::TextButton& btn, juce::Colour bgColour)
{
    btn.setColour (juce::TextButton::buttonColourId, bgColour);
    btn.setColour (juce::TextButton::buttonOnColourId, bgColour.brighter (0.2f));
    btn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible (btn);
}

void SH201AudioProcessorEditor::updatePitchSliderRange (const juce::String& pitchParamId, bool isWide)
{
    auto it = sliderMap.find (pitchParamId);
    if (it == sliderMap.end() || it->second == nullptr)
        return;

    auto* slider = it->second;
    double newMin = isWide ? -36.0 : -12.0;
    double newMax = isWide ? 36.0 : 12.0;

    if (slider->getMinimum() == newMin && slider->getMaximum() == newMax)
        return; // Already correct, avoid unnecessary updates

    double currentVal = slider->getValue();
    double clampedVal = juce::jlimit (newMin, newMax, currentVal);

    slider->setRange (newMin, newMax, 1.0);
    slider->setValue (clampedVal, juce::sendNotificationSync);
}

void SH201AudioProcessorEditor::updateAllPitchSliderRanges()
{
    for (const auto& prefix : { juce::String ("upper_"), juce::String ("lower_") })
    {
        for (const auto& oscSuffix : { juce::String ("osc1"), juce::String ("osc2") })
        {
            auto wideId = prefix + oscSuffix + "_pitch_wide";
            auto pitchId = prefix + oscSuffix + "_pitch";

            bool isWide = false;
            if (auto* wideVal = audioProcessor.getAPVTS().getRawParameterValue (wideId))
                isWide = *wideVal > 0.5f;

            updatePitchSliderRange (pitchId, isWide);
        }
    }
}

juce::Image SH201AudioProcessorEditor::createBrushedMetalImage (int width, int height)
{
    juce::Image img (juce::Image::RGB, width, height, true);
    juce::Random rng;

    // Draw horizontal lines of random lengths and slight darkness
    juce::Graphics g (img);
    g.fillAll (juce::Colour (0xffd1d5d9)); // Base silver

    for (int i = 0; i < 2000; ++i)
    {
        int y = rng.nextInt (height);
        int x = rng.nextInt (width);
        int length = rng.nextInt (40) + 10;
        float alpha = rng.nextFloat() * 0.08f + 0.02f; // subtle dark
        
        g.setColour (juce::Colours::black.withAlpha (alpha));
        g.drawLine ((float)x, (float)y, (float)(x + length), (float)y, 1.0f);
        
        // Wrap around lines if needed
        if (x + length > width)
            g.drawLine (0.0f, (float)y, (float)(x + length - width), (float)y, 1.0f);
    }

    // Add some highlights
    for (int i = 0; i < 1000; ++i)
    {
        int y = rng.nextInt (height);
        int x = rng.nextInt (width);
        int length = rng.nextInt (40) + 10;
        float alpha = rng.nextFloat() * 0.08f + 0.02f; // subtle light
        
        g.setColour (juce::Colours::white.withAlpha (alpha));
        g.drawLine ((float)x, (float)y, (float)(x + length), (float)y, 1.0f);
        
        if (x + length > width)
            g.drawLine (0.0f, (float)y, (float)(x + length - width), (float)y, 1.0f);
    }

    return img;
}

void SH201AudioProcessorEditor::drawSectionFrame (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title, const juce::String& badgeText, juce::Colour bgColour, juce::Colour textColour, juce::Colour borderColour)
{
    float pi = juce::MathConstants<float>::pi;
    float x = bounds.getX();
    float y = bounds.getY();
    float w = bounds.getWidth();
    float h = bounds.getHeight();
    
    float corner = 6.0f;
    float tabH = 28.0f; // Height of the file tab
    float tabSlope = 18.0f; // Sloped edge width
    
    juce::Font titleFont ("Impact", 18.0f, juce::Font::plain);
    float textW = title.length() * 11.0f; // Heuristic for 18pt Impact uppercase font
    float tabW = textW + 24.0f; // Give padding around title
    
    // Ensure tab doesn't overshoot the panel's right edge
    if (tabW + tabSlope > w - corner)
        tabW = w - corner - tabSlope;
    
    juce::Path p;
    // 1. Top-left corner of the tab
    p.addArc (x, y, corner * 2, corner * 2, pi * 1.5f, pi * 2.0f, true);
    // 2. Top edge of the tab
    p.lineTo (x + tabW, y);
    // 3. Slanted edge connecting tab to main body
    p.lineTo (x + tabW + tabSlope, y + tabH);
    // 4. Top edge of main body
    p.lineTo (x + w - corner, y + tabH);
    // 5. Top-right corner of main body
    p.addArc (x + w - corner * 2, y + tabH, corner * 2, corner * 2, 0.0f, pi * 0.5f, false);
    // 6. Bottom-right corner of main body
    p.addArc (x + w - corner * 2, y + h - corner * 2, corner * 2, corner * 2, pi * 0.5f, pi, false);
    // 7. Bottom-left corner of main body
    p.addArc (x, y + h - corner * 2, corner * 2, corner * 2, pi, pi * 1.5f, false);
    p.closeSubPath();
    
    if (bgColour == juce::Colour (0xffd1d5d9) && brushedMetalTexture.isValid()) // If it's the silver background
    {
        g.setTiledImageFill (brushedMetalTexture, 0, 0, 1.0f);
        g.fillPath (p);

        // Add a subtle vertical gradient on top for the "shiny" but non-reflective look
        juce::ColourGradient gradient (juce::Colours::white.withAlpha (0.4f), 0.0f, y,
                                       juce::Colours::black.withAlpha (0.3f), 0.0f, y + h, false);
        g.setGradientFill (gradient);
        g.fillPath (p);
    }
    else
    {
        g.setColour (bgColour);
        g.fillPath (p);
    }
    
    g.setColour (borderColour);
    g.strokePath (p, juce::PathStrokeType (1.5f));
    
    g.setColour (textColour);
    g.setFont (titleFont);
    // Draw title in the center of the tab
    g.drawText (title, (int)x + 12, (int)y + 2, (int)tabW, (int)tabH, juce::Justification::centredLeft, false);
    
    if (badgeText.isNotEmpty())
    {
        g.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::plain));
        g.drawText (badgeText, (int)x, (int)y + (int)tabH + 2, (int)w - 12, 14, juce::Justification::topRight, false);
    }
}

void SH201AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff252930));

    float scale = (float) getWidth() / 1280.0f;
    g.addTransform (juce::AffineTransform::scale (scale));

    // --- Top Header Panel ---
    g.setColour (juce::Colour (0xff2e333d));
    g.fillRect (0, 0, 1280, 56);
    g.setColour (juce::Colour (0xffb4afaa));
    g.fillRect (0, 55, 1280, 2);

    // Title Background (matches Osc Controls Panel style)
    g.setColour (juce::Colour (0xffd1d5d9));
    g.fillRoundedRectangle (15.0f, 6.0f, 190.0f, 26.0f, 4.0f);
    g.setColour (juce::Colour (0xffe2e5e8));
    g.drawRoundedRectangle (15.0f, 6.0f, 190.0f, 26.0f, 4.0f, 1.0f);

    // --- Bottom Footer Panel ---
    g.setColour (juce::Colour (0xff2e333d));
    g.fillRect (0, 920 - 28, 1280, 28);
    g.setColour (juce::Colour (0xff4a525e));
    g.fillRect (0, 920 - 28, 1280, 1);

    juce::Colour silverBg (0xffd1d5d9);
    juce::Colour silverBorder (0xffe2e5e8);
    juce::Colour silverText (0xff111111);

    juce::Colour darkBg (0xff363c45);
    juce::Colour darkBorder (0xff4a525e);
    juce::Colour darkText (0xffffffff);

    // --- STATIC COMMON / MASTER SECTION FRAMES (Always Visible) ---
    drawSectionFrame (g, { 15,  60,  270, 150 }, "PATCH COMMON", "", darkBg, darkText, darkBorder);
    drawSectionFrame (g, { 300, 60,  360, 150 }, "VOICE & PERFORM", "", darkBg, darkText, darkBorder);
    drawSectionFrame (g, { 675, 60,  270, 150 }, "MASTER EFFECTS", "", darkBg, darkText, darkBorder);
    drawSectionFrame (g, { 960, 60,  305, 150 }, "STEREO MODE (VST)", "", darkBg, darkText, darkBorder);

    // --- TONE PAGE TOGGLE BAR ---
    g.setColour (juce::Colour (0xff2e333d));
    g.fillRoundedRectangle (15.0f, 225.0f, 1250.0f, 50.0f, 6.0f);

    // --- DYNAMIC TONE SECTION FRAMES (Swaps between Upper / Lower) ---
    juce::String pageLabel = currentTonePage == TonePage::Upper ? "[UPPER TONE]" : "[LOWER TONE]";
    drawSectionFrame (g, { 15,  290, 390, 190 }, "OSCILLATOR 1", pageLabel, silverBg, silverText, silverBorder);
    drawSectionFrame (g, { 420, 290, 390, 190 }, "OSCILLATOR 2", pageLabel, silverBg, silverText, silverBorder);
    drawSectionFrame (g, { 825, 290, 240, 190 }, "MIX / MOD", pageLabel, silverBg, silverText, silverBorder);
    drawSectionFrame (g, { 1080, 290, 185, 190 }, "PITCH ENVELOPE", pageLabel, darkBg, darkText, darkBorder);

    drawSectionFrame (g, { 15,  495, 420, 214 }, "FILTER", pageLabel, silverBg, silverText, silverBorder);
    drawSectionFrame (g, { 450, 495, 470, 214 }, "FILTER ENVELOPE", pageLabel, darkBg, darkText, darkBorder);
    drawSectionFrame (g, { 935, 495, 330, 214 }, "AMP ENVELOPE", pageLabel, darkBg, darkText, darkBorder);

    drawSectionFrame (g, { 15,  724, 420, 190 }, "AMP & OVERDRIVE", pageLabel, silverBg, silverText, silverBorder);
    drawSectionFrame (g, { 450, 724, 405, 190 }, "LFO 1", pageLabel, darkBg, darkText, darkBorder);
    drawSectionFrame (g, { 870, 724, 395, 190 }, "LFO 2", pageLabel, darkBg, darkText, darkBorder);

    // Draw visual linking lines for Osc 1 and 2 pitch wide dropdowns
    auto drawPitchLine = [this, &g] (const juce::String& prefix, bool isOsc1)
    {
        auto wideId = prefix + (isOsc1 ? "osc1_pitch_wide" : "osc2_pitch_wide");
        auto pitchId = prefix + (isOsc1 ? "osc1_pitch" : "osc2_pitch");

        if (buttonMap.find (wideId) != buttonMap.end() && sliderMap.find (pitchId) != sliderMap.end() && labelMap.find (pitchId) != labelMap.end())
        {
            auto* button = buttonMap[wideId];
            auto* slider = sliderMap[pitchId];
            auto* pitchLabel = labelMap[pitchId];

            if (button != nullptr && slider != nullptr && pitchLabel != nullptr && button->isVisible())
            {
                auto cb = button->getBounds();
                auto sb = slider->getBounds();
                auto lb = pitchLabel->getBounds();

                float startX = (float) cb.getRight() + 4.0f;
                float startY = (float) cb.getCentreY();
                float endX = (float) sb.getCentreX();
                float endY = (float) lb.getY() - 4.0f; // End above the "PITCH" label

                juce::Path path;
                path.startNewSubPath (startX, startY);
                path.lineTo (endX, startY);
                path.lineTo (endX, endY);

                g.setColour (juce::Colours::black.withAlpha (0.7f));
                g.strokePath (path, juce::PathStrokeType (1.0f, juce::PathStrokeType::mitered)); // Thinner line
            }
        }
    };
    
    // Draw visual linking lines for Amp Overdrive
    auto drawOverdriveLine = [this, &g] (const juce::String& prefix)
    {
        auto switchId = prefix + "amp_overdrive_switch";
        auto driveId = prefix + "overdrive_level";

        if (buttonMap.find (switchId) != buttonMap.end() && sliderMap.find (driveId) != sliderMap.end() && labelMap.find (driveId) != labelMap.end())
        {
            auto* button = buttonMap[switchId];
            auto* slider = sliderMap[driveId];
            auto* label = labelMap[driveId];

            if (button != nullptr && slider != nullptr && label != nullptr && button->isVisible())
            {
                auto cb = button->getBounds();
                auto sb = slider->getBounds();
                auto lb = label->getBounds();

                float startX = (float) cb.getRight() + 4.0f;
                float startY = (float) cb.getCentreY();
                float endX = (float) sb.getCentreX();
                float endY = (float) lb.getY() - 4.0f; 

                juce::Path path;
                path.startNewSubPath (startX, startY);
                path.lineTo (endX, startY);
                path.lineTo (endX, endY);

                g.setColour (juce::Colours::black.withAlpha (0.7f));
                g.strokePath (path, juce::PathStrokeType (1.0f, juce::PathStrokeType::mitered));
            }
        }
    };
    
    juce::String activePrefix = currentTonePage == TonePage::Upper ? "upper_" : "lower_";
    drawPitchLine (activePrefix, true);
    drawPitchLine (activePrefix, false);
    drawOverdriveLine (activePrefix);
}

void SH201AudioProcessorEditor::resized()
{
    // --- Header Layout ---
    titleLabel.setBounds    (15,  8, 190, 22);
    subtitleLabel.setBounds (20,  34, 250, 16);
    statusLabel.setBounds       (250, 8, 400, 22);
    lastReceivedLabel.setBounds (250, 30, 400, 22);

    systemSettingsButton.setBounds (680, 13, 120, 28);

    pullPatchButton.setBounds      (915, 13, 95, 28);
    reconnectButton.setBounds      (1020, 13, 105, 28);

    if (patchBrowser)
        patchBrowser->setBounds (15, 55, 1280 - 30, 920 - 85);

    // --- Footer Layout ---

    // --- Tone Page Toggle Button Layout ---
    upperPageButton.setBounds    (300, 233, 240, 34);
    lowerPageButton.setBounds    (550, 233, 240, 34);
    initPatchButton.setBounds    (800, 233, 130, 34);
    patchBrowserButton.setBounds (940, 233, 160, 34);

    // Helper: find a slider or combo by paramId and set its bounds within a cell.
    // Also finds and positions the associated label above the control.
    auto placeControl = [this] (const juce::String& paramId, juce::Rectangle<int> cell)
    {
        int labelH = 18;

        // Position label first (shared by both slider and combo)
        auto lit = labelMap.find (paramId);
        if (lit != labelMap.end() && lit->second != nullptr)
            lit->second->setBounds (cell.getX(), cell.getY(), cell.getWidth(), labelH);

        auto sit = sliderMap.find (paramId);
        if (sit != sliderMap.end() && sit->second != nullptr)
        {
            auto* slider = sit->second;
            auto controlArea = cell.withTrimmedTop (labelH);
            if (slider->getSliderStyle() == juce::Slider::RotaryHorizontalVerticalDrag)
            {
                // Enforce uniform, consistent knob size across all grid cells
                int knobW = 54;
                int knobH = 66;
                int cx = controlArea.getCentreX() - (knobW / 2);
                int cy = controlArea.getCentreY() - (knobH / 2);
                slider->setBounds (cx, cy, knobW, knobH);
            }
            else
            {
                // Enforce uniform width for Linear ADSR envelope sliders
                int sliderW = 76;
                int cx = controlArea.getCentreX() - (sliderW / 2);
                slider->setBounds (cx, controlArea.getY(), sliderW, controlArea.getHeight() - 4);
            }
            return;
        }

        auto cit = comboMap.find (paramId);
        if (cit != comboMap.end() && cit->second != nullptr)
        {
            int comboH = 26;
            cit->second->setBounds (cell.getX() + 4, cell.getY() + labelH + 6, cell.getWidth() - 8, comboH);
        }
    };

    auto placeGrid = [&placeControl] (juce::Rectangle<int> box, const juce::StringArray& paramIds, int cols, int rows)
    {
        box.removeFromTop (42);  // Space for section file tab and right-aligned badge text
        box.reduce (6, 4);
        int cellW = box.getWidth() / cols;
        int cellH = box.getHeight() / rows;
        for (int i = 0; i < paramIds.size(); ++i)
        {
            int c = i % cols;
            int r = i / cols;
            auto cell = juce::Rectangle<int> (box.getX() + c * cellW, box.getY() + r * cellH, cellW, cellH);
            placeControl (paramIds[i], cell);
        }
    };

    auto placeToneGrid = [&placeGrid] (juce::Rectangle<int> box, const juce::StringArray& baseIds, int cols, int rows)
    {
        juce::StringArray upperIds, lowerIds;
        for (auto& id : baseIds)
        {
            upperIds.add ("upper_" + id);
            lowerIds.add ("lower_" + id);
        }
        placeGrid (box, upperIds, cols, rows);
        placeGrid (box, lowerIds, cols, rows);
    };

    auto placePatchCommon = [this] (juce::Rectangle<int> box)
    {
        box.removeFromTop (26);
        box.reduce (6, 4);

        int cellW = box.getWidth() / 3;
        int comboW = 86;
        int comboH = 26;
        int labelH = 18;
        int knobW = 68;
        int knobH = 84;
        
        juce::StringArray paramIds = { "common_keyboard_mode", "common_tone_balance", "common_split_point" };
        
        for (int i = 0; i < 3; ++i)
        {
            juce::Rectangle<int> cell = box.withX (box.getX() + i * cellW).withWidth (cellW);
            auto paramId = paramIds[i];
            
            if (labelMap.find (paramId) != labelMap.end())
                labelMap[paramId]->setBounds (cell.getX(), cell.getY(), cell.getWidth(), labelH);
                
            if (comboMap.find (paramId) != comboMap.end())
            {
                comboMap[paramId]->setBounds (cell.getCentreX() - comboW/2, cell.getY() + labelH + 6, comboW, comboH);
            }
            else if (sliderMap.find (paramId) != sliderMap.end())
            {
                auto controlArea = cell.withTrimmedTop (labelH);
                int cx = controlArea.getCentreX() - (knobW / 2);
                int cy = controlArea.getCentreY() - (knobH / 2);
                sliderMap[paramId]->setBounds (cx, cy, knobW, knobH);
                sliderMap[paramId]->setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobW, 18);
            }
        }
    };

    auto placeVoicePerform = [this] (juce::Rectangle<int> box)
    {
        box.removeFromTop (26);
        box.reduce (6, 4);

        int comboW = 86;
        int comboH = 26;
        int labelH = 18;
        int knobW = 68;
        int knobH = 84;
        
        juce::Rectangle<int> leftArea = box.removeFromLeft (90);
        
        auto setCombo = [this, comboW] (const juce::String& paramId, juce::Rectangle<int> cell) {
            if (labelMap.find (paramId) != labelMap.end())
                labelMap[paramId]->setBounds (cell.getX(), cell.getY(), cell.getWidth(), 18);
            if (comboMap.find (paramId) != comboMap.end())
                comboMap[paramId]->setBounds (cell.getCentreX() - comboW/2, cell.getY() + 18, comboW, 26);
        };
        
        juce::Rectangle<int> voiceCell = leftArea.removeFromTop (44);
        leftArea.removeFromTop (6);
        juce::Rectangle<int> portaCell = leftArea.removeFromTop (44);

        setCombo ("common_mono_solo_select", voiceCell);
        setCombo ("common_portamento_switch", portaCell);

        juce::StringArray knobIds = { "common_portamento_time", "common_pitch_bend_range", "common_octave_shift" };
        int cellW = box.getWidth() / 3;
        for (int i = 0; i < 3; ++i)
        {
            juce::Rectangle<int> cell = box.withX (box.getX() + i * cellW).withWidth (cellW);
            auto paramId = knobIds[i];
            
            if (labelMap.find (paramId) != labelMap.end())
                labelMap[paramId]->setBounds (cell.getX(), cell.getY(), cell.getWidth(), labelH);
                
            if (sliderMap.find (paramId) != sliderMap.end())
            {
                auto controlArea = cell.withTrimmedTop (labelH);
                int cx = controlArea.getCentreX() - (knobW / 2);
                int cy = controlArea.getCentreY() - (knobH / 2);
                sliderMap[paramId]->setBounds (cx, cy, knobW, knobH);
                sliderMap[paramId]->setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobW, 18);
            }
        }
    };

    auto placeMasterEffects = [this] (juce::Rectangle<int> box)
    {
        box.removeFromTop (26);
        box.reduce (6, 4);

        int comboW = 86;
        int labelH = 18;
        int knobW = 68;
        int knobH = 84;
        
        juce::Rectangle<int> leftArea = box.removeFromLeft (90);
        
        juce::Rectangle<int> dlyCell = leftArea.removeFromTop (32);
        leftArea.removeFromTop (12);
        juce::Rectangle<int> revCell = leftArea.removeFromTop (32);
        
        delaySwitchButton.setBounds (dlyCell.getCentreX() - comboW/2, dlyCell.getY(), comboW, 26);
        reverbSwitchButton.setBounds (revCell.getCentreX() - comboW/2, revCell.getY(), comboW, 26);
        
        advancedEffectsButton.setBounds (leftArea.getX() + 6, 180, 78, 22);

        juce::StringArray knobIds = { "common_delay_level", "common_reverb_level" };
        int cellW = box.getWidth() / 2;
        for (int i = 0; i < 2; ++i)
        {
            juce::Rectangle<int> cell = box.withX (box.getX() + i * cellW).withWidth (cellW);
            auto paramId = knobIds[i];
            
            if (labelMap.find (paramId) != labelMap.end())
                labelMap[paramId]->setBounds (cell.getX(), cell.getY(), cell.getWidth(), labelH);
                
            if (sliderMap.find (paramId) != sliderMap.end())
            {
                auto controlArea = cell.withTrimmedTop (labelH);
                int cx = controlArea.getCentreX() - (knobW / 2);
                int cy = controlArea.getCentreY() - (knobH / 2);
                sliderMap[paramId]->setBounds (cx, cy, knobW, knobH);
                sliderMap[paramId]->setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobW, 18);
            }
        }
    };

    // --- 1. STATIC COMMON / MASTER SECTION (y = 60..210) ---
    placePatchCommon ({ 15,  60,  270, 150 });
    advancedPatchCommonButton.setBounds (25, 180, 78, 22);

    placeVoicePerform ({ 300, 60,  360, 150 });
    placeMasterEffects ({ 675, 60,  270, 150 });

    // --- STEREO MODE SECTION ---
    stereoModeButton.setBounds (985, 100, 120, 32);

    auto lit = labelMap.find ("common_stereo_link_mode");
    if (lit != labelMap.end() && lit->second != nullptr)
        lit->second->setBounds (985, 146, 120, 18);

    auto cit = comboMap.find ("common_stereo_link_mode");
    if (cit != comboMap.end() && cit->second != nullptr)
        cit->second->setBounds (985, 166, 120, 26);
    {
        juce::Rectangle<int> cell { 1120, 92, 125, 110 };
        int labelH = 18;
        auto paramId = "common_stereo_width";
        if (labelMap.find (paramId) != labelMap.end() && sliderMap.find (paramId) != sliderMap.end())
        {
            labelMap[paramId]->setBounds (cell.getX(), cell.getY(), cell.getWidth(), labelH);
            auto controlArea = cell.withTrimmedTop (labelH);
            int knobW = 68;
            int knobH = 84;
            int cx = controlArea.getCentreX() - (knobW / 2);
            int cy = controlArea.getCentreY() - (knobH / 2);
            sliderMap[paramId]->setBounds (cx, cy, knobW, knobH);
            sliderMap[paramId]->setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobW, 18);
        }
    }

    auto placeOscPanels = [this] (juce::Rectangle<int> box, bool isOsc1)
    {
        box.removeFromTop (26); // Decreased from 42 to shift all controls up and prevent bottom overlap
        box.reduce (6, 4);

        int comboH = 26;
        int labelH = 18;
        int topSectionH = labelH + comboH + 10;
        juce::Rectangle<int> topArea = box.removeFromTop (topSectionH);
        
        int comboW = 100;
        int spacing = 30;
        int totalTopW = comboW * 2 + spacing;
        juce::Rectangle<int> combosBox = topArea.withSizeKeepingCentre (totalTopW, topArea.getHeight());
        
        juce::Rectangle<int> waveCell = combosBox.removeFromLeft (comboW);
        combosBox.removeFromLeft (spacing);
        juce::Rectangle<int> wideCell = combosBox;
        
        int knobW = 68;
        int knobH = 84;
        int knobSectionH = labelH + knobH; 
        juce::Rectangle<int> knobArea = box.withSizeKeepingCentre (box.getWidth(), knobSectionH);
        int cellW = knobArea.getWidth() / 4;

        juce::StringArray prefixes = { "upper_", "lower_" };
        for (auto& prefix : prefixes)
        {
            auto waveId = prefix + (isOsc1 ? "osc1_wave" : "osc2_wave");
            if (labelMap.find (waveId) != labelMap.end() && comboMap.find (waveId) != comboMap.end())
            {
                labelMap[waveId]->setBounds (waveCell.getX(), waveCell.getY(), waveCell.getWidth(), labelH);
                comboMap[waveId]->setBounds (waveCell.getX(), waveCell.getY() + labelH, waveCell.getWidth(), comboH);
            }

            auto wideId = prefix + (isOsc1 ? "osc1_pitch_wide" : "osc2_pitch_wide");
            if (buttonMap.find (wideId) != buttonMap.end())
            {
                int shortW = 54;
                buttonMap[wideId]->setBounds (wideCell.getRight() - shortW - 10, wideCell.getY() + labelH, shortW, comboH);
            }

            juce::StringArray knobIds;
            if (isOsc1)
                knobIds = { prefix + "osc1_fine_tune", prefix + "osc1_pulse_width", prefix + "osc1_pitch_env_depth", prefix + "osc1_pitch" };
            else
                knobIds = { prefix + "osc2_detune", prefix + "osc2_pulse_width", prefix + "osc2_pitch_env_depth", prefix + "osc2_pitch" };
                
            for (int i = 0; i < 4; ++i)
            {
                juce::Rectangle<int> cell = knobArea.withX (knobArea.getX() + i * cellW).withWidth (cellW);
                auto paramId = knobIds[i];
                if (labelMap.find (paramId) != labelMap.end() && sliderMap.find (paramId) != sliderMap.end())
                {
                    labelMap[paramId]->setBounds (cell.getX(), cell.getY(), cell.getWidth(), labelH);
                    
                    auto controlArea = cell.withTrimmedTop (labelH);
                    int cx = controlArea.getCentreX() - (knobW / 2);
                    int cy = controlArea.getCentreY() - (knobH / 2);
                    
                    sliderMap[paramId]->setBounds (cx, cy, knobW, knobH);
                    sliderMap[paramId]->setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobW, 18);
                }
            }
        }
    };

    auto placeFilterPanel = [this] (juce::Rectangle<int> box)
    {
        box.removeFromTop (26);
        box.reduce (6, 4);

        int comboH = 26;
        int labelH = 18;
        int topSectionH = labelH + comboH + 10;
        juce::Rectangle<int> topArea = box.removeFromTop (topSectionH);
        
        int comboW = 100;
        int spacing = 30;
        int totalTopW = comboW * 2 + spacing;
        juce::Rectangle<int> combosBox = topArea.withSizeKeepingCentre (totalTopW, topArea.getHeight());
        
        juce::Rectangle<int> typeCell = combosBox.removeFromLeft (comboW);
        combosBox.removeFromLeft (spacing);
        juce::Rectangle<int> slopeCell = combosBox;
        
        int knobW = 68;
        int knobH = 84;
        int knobSectionH = labelH + knobH; 
        juce::Rectangle<int> knobArea = box.withSizeKeepingCentre (box.getWidth(), knobSectionH);
        int cellW = knobArea.getWidth() / 4;

        juce::StringArray prefixes = { "upper_", "lower_" };
        for (auto& prefix : prefixes)
        {
            auto typeId = prefix + "filter_type";
            if (labelMap.find (typeId) != labelMap.end() && comboMap.find (typeId) != comboMap.end())
            {
                labelMap[typeId]->setBounds (typeCell.getX(), typeCell.getY(), typeCell.getWidth(), labelH);
                comboMap[typeId]->setBounds (typeCell.getX(), typeCell.getY() + labelH, typeCell.getWidth(), comboH);
            }

            auto slopeId = prefix + "filter_slope";
            if (labelMap.find (slopeId) != labelMap.end() && comboMap.find (slopeId) != comboMap.end())
            {
                labelMap[slopeId]->setBounds (slopeCell.getX(), slopeCell.getY(), slopeCell.getWidth(), labelH);
                comboMap[slopeId]->setBounds (slopeCell.getX(), slopeCell.getY() + labelH, slopeCell.getWidth(), comboH);
            }

            juce::StringArray knobIds = { prefix + "filter_cutoff", prefix + "filter_resonance", prefix + "filter_keyfollow", prefix + "filter_velocity_sens" };
                
            for (int i = 0; i < 4; ++i)
            {
                juce::Rectangle<int> cell = knobArea.withX (knobArea.getX() + i * cellW).withWidth (cellW);
                auto paramId = knobIds[i];
                if (labelMap.find (paramId) != labelMap.end() && sliderMap.find (paramId) != sliderMap.end())
                {
                    labelMap[paramId]->setBounds (cell.getX(), cell.getY(), cell.getWidth(), labelH);
                    
                    auto controlArea = cell.withTrimmedTop (labelH);
                    int cx = controlArea.getCentreX() - (knobW / 2);
                    int cy = controlArea.getCentreY() - (knobH / 2);
                    
                    sliderMap[paramId]->setBounds (cx, cy, knobW, knobH);
                    sliderMap[paramId]->setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobW, 18);
                }
            }
        }
    };

    auto placeAmpPanel = [this] (juce::Rectangle<int> box)
    {
        box.removeFromTop (26);
        box.reduce (6, 4);

        int comboH = 26;
        int labelH = 18;
        int topSectionH = labelH + comboH + 10;
        juce::Rectangle<int> topArea = box.removeFromTop (topSectionH);
        
        int knobW = 68;
        int knobH = 84;
        int knobSectionH = labelH + knobH; 
        juce::Rectangle<int> knobArea = box.withSizeKeepingCentre (box.getWidth(), knobSectionH);
        int cellW = knobArea.getWidth() / 4;

        int comboW = 100;
        juce::Rectangle<int> overdriveCell = topArea.withX (knobArea.getX() + 2 * cellW).withWidth (cellW);
        juce::Rectangle<int> switchCell = overdriveCell.withSizeKeepingCentre (comboW, topArea.getHeight());

        juce::StringArray prefixes = { "upper_", "lower_" };
        for (auto& prefix : prefixes)
        {
            auto switchId = prefix + "amp_overdrive_switch";
            if (buttonMap.find (switchId) != buttonMap.end())
            {
                buttonMap[switchId]->setBounds (switchCell.getX(), switchCell.getY() + labelH, switchCell.getWidth(), comboH);
            }

            juce::StringArray knobIds = { prefix + "amp_level", prefix + "amp_pan", prefix + "amp_velocity_sens", prefix + "overdrive_level" };
                
            for (int i = 0; i < 4; ++i)
            {
                juce::Rectangle<int> cell = knobArea.withX (knobArea.getX() + i * cellW).withWidth (cellW);
                auto paramId = knobIds[i];
                if (labelMap.find (paramId) != labelMap.end() && sliderMap.find (paramId) != sliderMap.end())
                {
                    labelMap[paramId]->setBounds (cell.getX(), cell.getY(), cell.getWidth(), labelH);
                    
                    auto controlArea = cell.withTrimmedTop (labelH);
                    int cx = controlArea.getCentreX() - (knobW / 2);
                    int cy = controlArea.getCentreY() - (knobH / 2);
                    
                    sliderMap[paramId]->setBounds (cx, cy, knobW, knobH);
                    sliderMap[paramId]->setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobW, 18);
                }
            }
        }
    };

    auto placeMixModPanel = [this] (juce::Rectangle<int> box)
    {
        box.removeFromTop (26);
        box.reduce (6, 4);

        int comboW = 100;
        int comboH = 26;
        int labelH = 18;
        int knobW = 68;
        int knobH = 84;
        
        juce::Rectangle<int> leftArea = box.removeFromLeft (box.getWidth() / 2);
        juce::Rectangle<int> rightArea = box;
        
        int dropdownBlockH = (labelH + comboH) * 2 + 10;
        juce::Rectangle<int> comboBlock = leftArea.withSizeKeepingCentre (comboW, dropdownBlockH);
        comboBlock.translate (10, 0); // Shift dropdowns slightly right
        
        juce::Rectangle<int> typeCell = comboBlock.removeFromTop (labelH + comboH);
        comboBlock.removeFromTop (10); // spacing
        juce::Rectangle<int> lowFreqCell = comboBlock.removeFromTop (labelH + comboH);

        int knobSectionH = labelH + knobH;
        juce::Rectangle<int> knobCell = rightArea.withSizeKeepingCentre (knobW, knobSectionH);

        juce::StringArray prefixes = { "upper_", "lower_" };
        for (auto& prefix : prefixes)
        {
            auto typeId = prefix + "mix_mod_type";
            if (labelMap.find (typeId) != labelMap.end() && comboMap.find (typeId) != comboMap.end())
            {
                labelMap[typeId]->setBounds (typeCell.getX(), typeCell.getY(), typeCell.getWidth(), labelH);
                comboMap[typeId]->setBounds (typeCell.getX(), typeCell.getY() + labelH, typeCell.getWidth(), comboH);
            }

            auto lowFreqId = prefix + "mix_mod_low_freq";
            if (labelMap.find (lowFreqId) != labelMap.end() && comboMap.find (lowFreqId) != comboMap.end())
            {
                labelMap[lowFreqId]->setBounds (lowFreqCell.getX(), lowFreqCell.getY(), lowFreqCell.getWidth(), labelH);
                comboMap[lowFreqId]->setBounds (lowFreqCell.getX(), lowFreqCell.getY() + labelH, lowFreqCell.getWidth(), comboH);
            }

            auto balId = prefix + "mix_mod_balance";
            if (labelMap.find (balId) != labelMap.end() && sliderMap.find (balId) != sliderMap.end())
            {
                labelMap[balId]->setBounds (knobCell.getX(), knobCell.getY(), knobCell.getWidth(), labelH);
                
                auto controlArea = knobCell.withTrimmedTop (labelH);
                int cx = controlArea.getCentreX() - (knobW / 2);
                int cy = controlArea.getCentreY() - (knobH / 2);
                
                sliderMap[balId]->setBounds (cx, cy, knobW, knobH);
                sliderMap[balId]->setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobW, 18);
            }
        }
    };

    // --- 2. DYNAMIC TONE PAGE SECTION (y = 290..840) ---
    placeOscPanels ({ 15,  290, 390, 190 }, true);
    placeOscPanels ({ 420, 290, 390, 190 }, false);
    placeMixModPanel ({ 825, 290, 240, 190 });
    placeToneGrid ({ 1080, 290, 185, 190 }, { "pitch_env_attack", "pitch_env_decay" }, 2, 1);

    placeFilterPanel ({ 15,  495, 420, 214 });
    placeToneGrid ({ 450, 495, 470, 214 }, { "filter_attack", "filter_decay", "filter_sustain", "filter_release", "filter_env_depth" }, 5, 1);
    placeToneGrid ({ 935, 495, 330, 214 }, { "amp_attack", "amp_decay", "amp_sustain", "amp_release" }, 4, 1);

    auto placeLfoPanel = [this] (juce::Rectangle<int> box, const juce::String& lfoPrefix)
    {
        box.removeFromTop (12);
        box.reduce (8, 0);

        int colW = box.getWidth() / 5;
        int comboW = 86;
        int knobW = 68;
        int knobH = 84;
        
        int buttonW = 60;
        
        juce::Rectangle<int> col1 = box.removeFromLeft (colW);
        juce::Rectangle<int> col2 = box.removeFromLeft (colW);
        juce::Rectangle<int> col3 = box.removeFromLeft (colW);
        juce::Rectangle<int> col4 = box.removeFromLeft (colW);
        juce::Rectangle<int> col5 = box; // remaining
        
        juce::StringArray layerPrefixes = { "upper_", "lower_" };
        
        for (const auto& layerPrefix : layerPrefixes)
        {
            auto getParamId = [layerPrefix, lfoPrefix] (const juce::String& base) {
                return layerPrefix + lfoPrefix + base;
            };
            
            auto placeCombo = [this, comboW] (const juce::String& paramId, int cx, int y) {
                if (labelMap.find (paramId) != labelMap.end())
                    labelMap[paramId]->setBounds (cx - comboW/2, y, comboW, 18);
                if (comboMap.find (paramId) != comboMap.end())
                    comboMap[paramId]->setBounds (cx - comboW/2, y + 18, comboW, 26);
            };
            
            auto placeButton = [this] (const juce::String& paramId, int cx, int y, int bw) {
                if (buttonMap.find (paramId) != buttonMap.end())
                    buttonMap[paramId]->setBounds (cx - bw/2, y + 18, bw, 26);
            };
            
            auto placeKnob = [this, knobW, knobH] (const juce::String& paramId, int cx, int cy) {
                if (sliderMap.find (paramId) != sliderMap.end())
                {
                    sliderMap[paramId]->setBounds (cx - knobW/2, cy - knobH/2, knobW, knobH);
                    sliderMap[paramId]->setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobW, 16);
                }
                if (labelMap.find (paramId) != labelMap.end())
                    labelMap[paramId]->setBounds (cx - knobW/2, cy - knobH/2 - 8, knobW, 18);
            };
            
            auto placeSlider = [this] (const juce::String& paramId, int cx, int y) {
                int sliderW = 76;
                int sliderH = 120;
                if (labelMap.find (paramId) != labelMap.end())
                    labelMap[paramId]->setBounds (cx - sliderW/2, y + 10, sliderW, 18);
                if (sliderMap.find (paramId) != sliderMap.end())
                    sliderMap[paramId]->setBounds (cx - sliderW/2, y + 28, sliderW, sliderH);
            };

            // Col 1 (Moved down)
            placeCombo  (getParamId ("_shape"), col1.getCentreX(), col1.getY() + 18);
            placeCombo  (getParamId ("_tempo_note"), col1.getCentreX(), col1.getY() + 66);
            placeButton (getParamId ("_tempo_sync"), col1.getCentreX(), col1.getY() + 114, buttonW);
            
            // Col 2 (Aligned KEYTRIG with RATE knob)
            placeButton (getParamId ("_key_trigger"), col2.getCentreX() + 4, col2.getY() + 18, buttonW);
            placeKnob   (getParamId ("_rate"), col2.getCentreX() + 4, col2.getY() + 118);
            
            // Col 3 (Lowered knob to center, reverted dest1 height)
            placeKnob   (getParamId ("_depth"), col3.getCentreX() + 12, col3.getY() + 82);
            placeCombo  (getParamId ("_dest1"), col3.getCentreX() + 12, col3.getY() + 124);
            
            // Col 4 (Lowered knob to center, reverted dest2 height)
            placeKnob   (getParamId ("_depth2"), col4.getCentreX() + 24, col4.getY() + 82);
            placeCombo  (getParamId ("_dest2"), col4.getCentreX() + 24, col4.getY() + 124);
            
            // Col 5
            placeSlider (getParamId ("_fade_time"), col5.getCentreX() + 18, col5.getY() + 22);
        }
    };

    placeAmpPanel ({ 15,  724, 420, 190 });
    placeLfoPanel ({ 450, 724, 405, 190 }, "lfo1");
    placeLfoPanel ({ 870, 724, 395, 190 }, "lfo2");

    auto positionLinkButton = [this] (int index, juce::Rectangle<int> bounds)
    {
        if (index < linkButtons.size() && linkButtons[index] != nullptr)
        {
            int bw = 42;
            int bh = 18;
            linkButtons[index]->setBounds (bounds.getX() + bounds.getWidth() - bw - 10, bounds.getY() + 5, bw, bh);
        }
    };
    
    positionLinkButton (0, { 15,  290, 390, 190 }); // OSC 1
    positionLinkButton (1, { 420, 290, 390, 190 }); // OSC 2
    positionLinkButton (2, { 825, 290, 240, 190 }); // MIX / MOD
    positionLinkButton (3, { 1080, 290, 185, 190 }); // PITCH ENV

    positionLinkButton (4, { 15,  495, 420, 214 }); // FILTER
    positionLinkButton (5, { 450, 495, 470, 214 }); // FILTER ENV
    positionLinkButton (6, { 935, 495, 330, 214 }); // AMP ENV

    positionLinkButton (7, { 15,  724, 420, 190 }); // AMP & OVERDRIVE
    positionLinkButton (8, { 450, 724, 405, 190 }); // LFO 1
    positionLinkButton (9, { 870, 724, 395, 190 }); // LFO 2

    float scale = (float) getWidth() / 1280.0f;
    for (auto* child : getChildren())
    {
        if (dynamic_cast<juce::ResizableCornerComponent*> (child) == nullptr)
        {
            child->setTransform (juce::AffineTransform::scale (scale));
        }
    }

    if (editorIsReady && getWidth() > 0 && getHeight() > 0)
    {
        // Only save if the user is actively resizing via mouse, to prevent the DAW from corrupting the save on deletion/unmap
        if (juce::ModifierKeys::getCurrentModifiers().isAnyMouseButtonDown())
        {
            GlobalSettings::setGuiSize (getWidth(), getHeight());
        }
    }
}

void SH201AudioProcessorEditor::timerCallback()
{
    bool connected = audioProcessor.isHardwareConnected();

    if (connected)
    {
        if (audioProcessor.isSyncingFromHardware.load())
        {
            statusLabel.setText ("SYNCING TO HARDWARE...", juce::dontSendNotification);
            statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xffffcc00));
        }
        else
        {
            statusLabel.setText (audioProcessor.getHardwareConnectionStatus(), juce::dontSendNotification);
            statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff00ff88));
        }
    }
    else
    {
        statusLabel.setText (audioProcessor.getHardwareConnectionStatus(), juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xffff3355));
    }

    int rxCC = audioProcessor.lastRxCC.load();
    int rxVal = audioProcessor.lastRxVal.load();
    if (rxCC >= 0)
    {
        juce::String rxText = "LAST HW RX: " + audioProcessor.getLastRxParamName() + " (CC#" + juce::String (rxCC) + " = " + juce::String (rxVal) + ")";
        if (audioProcessor.isStereoModeActive())
            rxText += " [STEREO MIRROR]";
        lastReceivedLabel.setText (rxText, juce::dontSendNotification);
    }
    else
        lastReceivedLabel.setText ("LAST HW RX: [Waiting for knob turn...]", juce::dontSendNotification);

    // Keep pitch slider ranges in sync with the wide button state
    // (handles hardware syncs where pitch_wide may change externally)
    updateAllPitchSliderRanges();

}
