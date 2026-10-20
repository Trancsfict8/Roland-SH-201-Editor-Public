#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Patches/PatchBrowserComponent.h"
#include "BinaryData.h"
#include <map>

class SH201LookAndFeel : public juce::LookAndFeel_V4
{
public:
    SH201LookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        setColour (juce::Slider::thumbColourId, juce::Colour (0xff8893a0));
        setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffffffff));
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff999999));

        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2e333d));
        setColour (juce::ComboBox::textColourId, juce::Colours::white);
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff555555));
        setColour (juce::ComboBox::arrowColourId, juce::Colours::white);

        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff252930));
        setColour (juce::PopupMenu::textColourId, juce::Colours::white);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff4a5261));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

        setColour (juce::DirectoryContentsDisplayComponent::highlightColourId, juce::Colour (0x38ffffff));
        setColour (juce::DirectoryContentsDisplayComponent::highlightedTextColourId, juce::Colours::white);
        setColour (juce::DirectoryContentsDisplayComponent::textColourId, juce::Colour (0xffcccccc));

        knobStrip = juce::ImageCache::getFromMemory (BinaryData::_201_knob_strip_png, BinaryData::_201_knob_strip_pngSize);
        sliderStrip  = juce::ImageCache::getFromMemory (BinaryData::_201slider_png, BinaryData::_201slider_pngSize);
    }

    void drawFileBrowserRow (juce::Graphics& g, int width, int height,
                             const juce::File& file, const juce::String& filename,
                             juce::Image* icon, const juce::String& fileSizeDescription,
                             const juce::String& fileTimeDescription,
                             bool isDirectory, bool isItemSelected,
                             int itemIndex, juce::DirectoryContentsDisplayComponent& dcc) override
    {
        auto fileListComp = dynamic_cast<juce::Component*> (&dcc);

        if (isItemSelected)
        {
            g.fillAll (juce::Colour (0x38ffffff));
            g.setColour (juce::Colour (0xffb0b8c0));
            g.drawRect (juce::Rectangle<float> (1.0f, 1.0f, (float) width - 2.0f, (float) height - 2.0f), 2.0f);
        }

        const int x = 32;

        if (icon != nullptr && icon->isValid())
        {
            g.drawImageWithin (*icon, 4, 4, x - 8, height - 8,
                               juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize,
                               false);
        }

        if (isItemSelected)
            g.setColour (juce::Colours::white);
        else
            g.setColour (juce::Colour (0xffcccccc));

        g.setFont (juce::Font ("Segoe UI", (float) height * 0.70f, isItemSelected ? juce::Font::bold : juce::Font::plain));

        if (width > 350 && ! isDirectory)
        {
            auto sizeX = juce::roundToInt ((float) width * 0.60f);
            auto dateX = juce::roundToInt ((float) width * 0.78f);

            g.drawFittedText (filename, x, 0, sizeX - x, height, juce::Justification::centredLeft, 1);

            g.setFont (juce::Font ("Segoe UI", (float) height * 0.55f, juce::Font::plain));
            g.setColour (juce::Colour (0xff888888));
            g.drawFittedText (fileSizeDescription, sizeX, 0, dateX - sizeX, height, juce::Justification::centredLeft, 1);
            g.drawFittedText (fileTimeDescription, dateX, 0, width - dateX - 6, height, juce::Justification::centredLeft, 1);
        }
        else
        {
            g.drawFittedText (filename, x, 0, width - x - 6, height, juce::Justification::centredLeft, 1);
        }
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider) override
    {
        if (knobStrip.isValid() && ! slider.getProperties().contains ("isVectorKnob"))
        {
            const int numFrames = knobStrip.getHeight() / knobStrip.getWidth();
            const int frameIdx = (int) std::round (sliderPos * (numFrames - 1));
            
            const int frameHeight = knobStrip.getWidth(); // Square frames assumed
            
            const float radius = juce::jmin (width / 2.0f, height / 2.0f) * 0.9f;
            const float destSize = radius * 2.0f;
            const float destX = x + width * 0.5f - radius;
            const float destY = y + height * 0.5f + 5.0f - radius; // pushed down by 5px like original
            
            g.drawImage (knobStrip,
                         (int) destX, (int) destY, (int) destSize, (int) destSize,
                         0, frameIdx * frameHeight, knobStrip.getWidth(), frameHeight);
        }
        else
        {
            auto radius = (float) juce::jmin (width / 2, height / 2) - 3.0f;
            auto centreX = (float) x + (float) width  * 0.5f;
            auto centreY = (float) y + (float) height * 0.5f;
            
            centreY += 5.0f; 

            auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
            
            g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
            juce::Path track;
            track.addCentredArc (centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
            g.strokePath (track, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            
            g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
            juce::Path fill;
            fill.addCentredArc (centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, angle, true);
            g.strokePath (fill, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            
            float thumbRadius = 3.5f;
            
            juce::Point<float> thumbPoint (centreX + radius * std::cos (angle - juce::MathConstants<float>::halfPi),
                                           centreY + radius * std::sin (angle - juce::MathConstants<float>::halfPi));
                                           
            g.setColour (slider.findColour (juce::Slider::thumbColourId));
            g.fillEllipse (thumbPoint.x - thumbRadius, thumbPoint.y - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f);
        }
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (slider.isBar())
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
            return;
        }

        if (sliderStrip.isValid())
        {
            const int numFrames = sliderStrip.getHeight() / sliderStrip.getWidth();
            double proportion = (slider.getValue() - slider.getMinimum()) / (slider.getMaximum() - slider.getMinimum());
            const int frameIdx = (int) std::round (proportion * (numFrames - 1));
            
            const int frameHeight = sliderStrip.getWidth(); 
            
            // Draw the frame scaled to fit the slider bounds
            g.drawImage (sliderStrip,
                         x, y, width, height,
                         0, frameIdx * frameHeight, sliderStrip.getWidth(), frameHeight);
        }
        else
        {
            auto trackWidth = 6.0f;
            juce::Point<float> startPoint ((float) x + (float) width * 0.5f, (float) (height + y));
            juce::Point<float> endPoint ((float) x + (float) width * 0.5f, (float) y);

            juce::Path backgroundTrack;
            backgroundTrack.startNewSubPath (startPoint);
            backgroundTrack.lineTo (endPoint);
            g.setColour (slider.findColour (juce::Slider::backgroundColourId));
            g.strokePath (backgroundTrack, { trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

            juce::Path valueTrack;
            valueTrack.startNewSubPath (startPoint);
            valueTrack.lineTo (startPoint.x, sliderPos);
            g.setColour (slider.findColour (juce::Slider::trackColourId));
            g.strokePath (valueTrack, { trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

            auto thumbWidth = getSliderThumbRadius (slider);
            g.setColour (slider.findColour (juce::Slider::thumbColourId));
            g.fillEllipse (juce::Rectangle<float> (static_cast<float> (thumbWidth), static_cast<float> (thumbWidth)).withCentre ({startPoint.x, sliderPos}));
        }
    }

private:
    juce::Image knobStrip;
    juce::Image sliderStrip;
};

class SH201AudioProcessorEditor : public juce::AudioProcessorEditor,
                                  public juce::Timer
{
public:
    explicit SH201AudioProcessorEditor (SH201AudioProcessor&);
    ~SH201AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    SH201LookAndFeel customLookAndFeel;
    SH201AudioProcessor& audioProcessor;

    // Header & Status UI
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label statusLabel;
    juce::Label lastReceivedLabel;
    juce::TextButton reconnectButton;
    juce::TextButton systemSettingsButton;
    juce::TextButton patchBrowserButton;

    juce::Image brushedMetalTexture;
    juce::Image createBrushedMetalImage (int width, int height);

    bool editorIsReady = false;

    juce::TextButton pullPatchButton;
    juce::TextButton advancedPatchCommonButton;
    juce::TextButton advancedEffectsButton;

    // Tone Page Toggle UI
    enum class TonePage { Upper, Lower };
    TonePage currentTonePage = TonePage::Upper;

    enum class ViewMode { Editor, Browser };
    ViewMode currentViewMode = ViewMode::Editor;

    juce::TextButton upperPageButton;
    juce::TextButton lowerPageButton;
    juce::TextButton initPatchButton;
    juce::TextButton stereoModeButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> stereoModeAttachment;

    juce::TextButton delaySwitchButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> delaySwitchAttachment;
    juce::GlowEffect delayGlow;
    
    juce::TextButton reverbSwitchButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> reverbSwitchAttachment;
    juce::GlowEffect reverbGlow;

    std::unique_ptr<PatchBrowserComponent> patchBrowser;

    void updatePageButtons();
    void updateTonePageVisibility();
    void updateViewModeVisibility();


    // Dynamic Parameter GUI storage
    juce::OwnedArray<juce::TextButton> linkButtons;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> linkAttachments;

    juce::OwnedArray<juce::TextButton> toggleButtons;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> toggleAttachments;
    juce::OwnedArray<juce::GlowEffect> toggleGlows;

    juce::OwnedArray<juce::Label> envLabels;
    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::ComboBox> comboBoxes;
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachments;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachments;

    std::map<juce::String, juce::Slider*> sliderMap;
    std::map<juce::String, juce::ComboBox*> comboMap;
    std::map<juce::String, juce::Label*> labelMap;
    std::map<juce::String, juce::TextButton*> buttonMap;

    juce::Slider* addRotarySlider (const juce::String& paramId, const juce::String& labelText);
    juce::Slider* addLinearSlider (const juce::String& paramId, const juce::String& labelText);
    juce::ComboBox* addComboBox   (const juce::String& paramId, const juce::String& labelText, const juce::StringArray& items);

    // Helper styling functions
    void styleRotarySlider (juce::Slider& slider, juce::Label& label, const juce::String& paramId, const juce::String& name);
    void styleLinearSlider (juce::Slider& slider, juce::Label& label, const juce::String& paramId, const juce::String& name);
    void styleComboBox     (juce::ComboBox& combo, juce::Label& label, const juce::String& paramId, const juce::String& name);
    void styleButton       (juce::TextButton& btn, juce::Colour bgColour);

    void updatePitchSliderRange (const juce::String& pitchParamId, bool isWide);
    void updateAllPitchSliderRanges();

    void drawSectionFrame (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title, 
                           const juce::String& badgeText = "",
                           juce::Colour bgColour = juce::Colour (0xff161a22), 
                           juce::Colour textColour = juce::Colour (0xff00e5ff), 
                           juce::Colour borderColour = juce::Colour (0xff28303d));

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SH201AudioProcessorEditor)
};
