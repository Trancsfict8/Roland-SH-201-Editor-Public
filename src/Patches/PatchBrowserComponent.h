#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "SH201PatchSerializer.h"

class OnlyFoldersFilter : public juce::FileFilter
{
public:
    OnlyFoldersFilter() : juce::FileFilter ("Only Folders") {}
    bool isFileSuitable (const juce::File&) const override { return false; }
    bool isDirectorySuitable (const juce::File&) const override { return true; }
};

class OnlySyxFilesFilter : public juce::FileFilter
{
public:
    OnlySyxFilesFilter() : juce::FileFilter ("Only Patch Files") {}
    bool isFileSuitable (const juce::File& file) const override { return file.hasFileExtension (".syx") || file.hasFileExtension (".mid") || file.hasFileExtension(".midi"); }
    bool isDirectorySuitable (const juce::File&) const override { return false; }
};

class PatchBrowserComponent : public juce::Component,
                              public juce::FileBrowserListener,
                              public juce::Timer
{
public:
    explicit PatchBrowserComponent (SH201AudioProcessor&, juce::LookAndFeel&, std::function<void()> onClose);
    ~PatchBrowserComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    // FileBrowserListener callbacks
    void selectionChanged() override;
    void fileClicked (const juce::File& file, const juce::MouseEvent& e) override;
    void fileDoubleClicked (const juce::File& file) override;
    void browserRootChanged (const juce::File& newRoot) override;

    void refreshBrowser();

private:
    SH201AudioProcessor& processor;
    juce::LookAndFeel& lookAndFeel;
    std::function<void()> onCloseCallback;

    OnlyFoldersFilter onlyFoldersFilter;
    OnlySyxFilesFilter onlySyxFilesFilter;
    juce::TimeSliceThread directoryThread { "PatchBrowserThread" };
    std::unique_ptr<juce::DirectoryContentsList> folderContentsList;
    std::unique_ptr<juce::DirectoryContentsList> fileContentsList;

    std::unique_ptr<juce::FileTreeComponent> folderTree;
    std::unique_ptr<juce::FileListComponent> fileList;

    juce::Label titleLabel;
    juce::Label pathLabel;
    juce::Label targetToneBadge;
    juce::Label statusLabel;

    juce::TextButton newFolderButton;
    juce::TextButton exportButton;
    juce::TextButton closeButton;
    juce::TextButton refreshButton;

    juce::File currentFolder;
    juce::File selectedFile;

    void handleNewFolder();
    void handleExportPatch();
    void handleLoadPatch();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatchBrowserComponent)
};
