#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class GlobalSettings
{
public:
    static juce::PropertiesFile::Options getOptions()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "SH201Editor";
        options.filenameSuffix = ".xml";
        options.folderName = "Transfict8 Audio Tools";
        options.osxLibrarySubFolder = "Application Support";
        options.commonToAllUsers = false;
        options.ignoreCaseOfKeyNames = true;
        options.millisecondsBeforeSaving = 1000;
        return options;
    }

    static juce::File getSettingsFile()
    {
        auto newFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("Roland SH-201 Editor");
        auto newFile = newFolder.getChildFile ("SH201Editor.xml");

        if (! newFile.existsAsFile())
        {
            newFolder.createDirectory();

            // 1. Check if legacy XML exists in the new folder under the old name
            auto oldNamedFileInNewFolder = newFolder.getChildFile ("SH201TotalIntegration.xml");
            if (oldNamedFileInNewFolder.existsAsFile())
            {
                oldNamedFileInNewFolder.moveFileTo (newFile);
                return newFile;
            }

            // 2. Check legacy folder with old name and copy settings
            auto oldFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                .getChildFile ("Roland SH201 Total Integration");
            auto oldFile = oldFolder.getChildFile ("SH201TotalIntegration.xml");

            if (oldFile.existsAsFile())
            {
                oldFile.copyFileTo (newFile);
                return newFile;
            }
        }

        return newFile;
    }

    static void setGuiSize (int width, int height)
    {
        juce::File file = getSettingsFile();
        file.getParentDirectory().createDirectory(); // Ensure folder exists
        juce::PropertiesFile props (file, getOptions());
        props.setValue ("gui_width", width);
        props.setValue ("gui_height", height);
        props.save(); // Force synchronous save
    }

    static void getGuiSize (int& width, int& height, int defaultWidth = 1280, int defaultHeight = 920)
    {
        juce::PropertiesFile props (getSettingsFile(), getOptions());
        width = props.getIntValue ("gui_width", defaultWidth);
        height = props.getIntValue ("gui_height", defaultHeight);
    }

    static void setEditorMode (int mode)
    {
        juce::File file = getSettingsFile();
        file.getParentDirectory().createDirectory(); // Ensure folder exists
        juce::PropertiesFile props (file, getOptions());
        props.setValue ("editor_mode", mode);
        props.save(); // Force synchronous save
    }

    static int getEditorMode (int defaultMode = 0)
    {
        juce::PropertiesFile props (getSettingsFile(), getOptions());
        return props.getIntValue ("editor_mode", defaultMode);
    }

    static void setDinMidiIn (const juce::String& identifier, const juce::String& name = {})
    {
        juce::File file = getSettingsFile();
        file.getParentDirectory().createDirectory();
        juce::PropertiesFile props (file, getOptions());
        props.setValue ("din_midi_in", identifier);
        if (name.isNotEmpty())
            props.setValue ("din_midi_in_name", name);
        props.save();
    }

    static juce::String getDinMidiIn (const juce::String& defaultVal = {})
    {
        juce::PropertiesFile props (getSettingsFile(), getOptions());
        return props.getValue ("din_midi_in", defaultVal);
    }

    static juce::String getDinMidiInName (const juce::String& defaultVal = {})
    {
        juce::PropertiesFile props (getSettingsFile(), getOptions());
        return props.getValue ("din_midi_in_name", defaultVal);
    }

    static void setDinMidiOut (const juce::String& identifier, const juce::String& name = {})
    {
        juce::File file = getSettingsFile();
        file.getParentDirectory().createDirectory();
        juce::PropertiesFile props (file, getOptions());
        props.setValue ("din_midi_out", identifier);
        if (name.isNotEmpty())
            props.setValue ("din_midi_out_name", name);
        props.save();
    }

    static juce::String getDinMidiOut (const juce::String& defaultVal = {})
    {
        juce::PropertiesFile props (getSettingsFile(), getOptions());
        return props.getValue ("din_midi_out", defaultVal);
    }

    static juce::String getDinMidiOutName (const juce::String& defaultVal = {})
    {
        juce::PropertiesFile props (getSettingsFile(), getOptions());
        return props.getValue ("din_midi_out_name", defaultVal);
    }
};
