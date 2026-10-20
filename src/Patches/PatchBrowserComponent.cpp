#include "PatchBrowserComponent.h"

PatchBrowserComponent::PatchBrowserComponent (SH201AudioProcessor& p, juce::LookAndFeel& lf, std::function<void()> onClose)
    : processor (p), lookAndFeel (lf), onCloseCallback (std::move (onClose))
{
    SH201PatchSerializer::ensurePatchesDirectoryExists();
    currentFolder = SH201PatchSerializer::getPatchesRootDirectory();

    directoryThread.startThread (juce::Thread::Priority::normal);

    folderContentsList = std::make_unique<juce::DirectoryContentsList> (&onlyFoldersFilter, directoryThread);
    folderContentsList->setDirectory (currentFolder, true, false);

    folderTree = std::make_unique<juce::FileTreeComponent> (*folderContentsList);
    folderTree->addListener (this);
    folderTree->setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (*folderTree);

    fileContentsList = std::make_unique<juce::DirectoryContentsList> (&onlySyxFilesFilter, directoryThread);
    fileContentsList->setDirectory (currentFolder, false, true);

    fileList = std::make_unique<juce::FileListComponent> (*fileContentsList);
    fileList->addListener (this);
    fileList->setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (*fileList);

    titleLabel.setText ("PATCH BROWSER & PRESET MANAGER", juce::dontSendNotification);
    titleLabel.setFont (juce::Font ("Segoe UI", 16.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff00e5ff));
    addAndMakeVisible (titleLabel);

    pathLabel.setText ("CURRENT FOLDER:  " + currentFolder.getFullPathName(), juce::dontSendNotification);
    pathLabel.setFont (juce::Font ("Segoe UI", 15.0f, juce::Font::bold));
    pathLabel.setColour (juce::Label::textColourId, juce::Colour (0xffffffff));
    addAndMakeVisible (pathLabel);

    targetToneBadge.setFont (juce::Font ("Segoe UI", 12.0f, juce::Font::bold));
    targetToneBadge.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (targetToneBadge);

    statusLabel.setFont (juce::Font ("Segoe UI", 12.0f, juce::Font::plain));
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (statusLabel);

    newFolderButton.setButtonText ("+ NEW FOLDER");
    newFolderButton.onClick = [this] { handleNewFolder(); };
    addAndMakeVisible (newFolderButton);

    exportButton.setButtonText ("EXPORT CURRENT PATCH (.MID)...");
    exportButton.onClick = [this] { handleExportPatch(); };
    addAndMakeVisible (exportButton);

    refreshButton.setButtonText ("REFRESH");
    refreshButton.onClick = [this] { refreshBrowser(); };
    addAndMakeVisible (refreshButton);

    closeButton.setButtonText ("CLOSE BROWSER");
    closeButton.onClick = [this] {
        if (onCloseCallback)
            onCloseCallback();
    };
    addAndMakeVisible (closeButton);

    startTimer (200);

    // Rule 22: setSize at the very end of constructor
    setSize (800, 500);
}

PatchBrowserComponent::~PatchBrowserComponent()
{
    stopTimer();
    directoryThread.stopThread (1000);
}

void PatchBrowserComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff161a22));

    g.setColour (juce::Colour (0xff28303d));
    g.drawRect (getLocalBounds(), 1);

    // Left and Right pane borders
    if (folderTree && fileList)
    {
        g.setColour (juce::Colour (0xff222833));
        g.drawRect (folderTree->getBounds().expanded (1));
        g.drawRect (fileList->getBounds().expanded (1));
    }
}

void PatchBrowserComponent::resized()
{
    auto area = getLocalBounds().reduced (12);

    // Header (32 px)
    auto headerArea = area.removeFromTop (32);
    titleLabel.setBounds (headerArea.removeFromLeft (320));
    closeButton.setBounds (headerArea.removeFromRight (120));
    refreshButton.setBounds (headerArea.removeFromRight (80).reduced (2, 4));

    // Path bar (30 px) - larger font for readability
    auto pathArea = area.removeFromTop (30);
    pathLabel.setBounds (pathArea);

    // Footer (36 px)
    auto footerArea = area.removeFromBottom (36);
    targetToneBadge.setBounds (footerArea.removeFromLeft (240));
    newFolderButton.setBounds (footerArea.removeFromLeft (130).reduced (0, 4));
    exportButton.setBounds (footerArea.removeFromRight (260).reduced (0, 4));
    statusLabel.setBounds (footerArea.reduced (10, 0));

    area.removeFromBottom (8); // spacing

    // Two-Pane Explorer Bounds
    auto leftPaneArea = area.removeFromLeft (juce::jmax (220, area.getWidth() / 3));
    area.removeFromLeft (10); // divider spacing

    if (folderTree)
        folderTree->setBounds (leftPaneArea);
    if (fileList)
        fileList->setBounds (area);
}

void PatchBrowserComponent::timerCallback()
{
    int activePage = processor.getCurrentTonePage();
    if (activePage == 0)
    {
        targetToneBadge.setText ("TARGET: UPPER TONE [0x01]", juce::dontSendNotification);
        targetToneBadge.setColour (juce::Label::textColourId, juce::Colour (0xffe74c3c)); // Upper red
    }
    else
    {
        targetToneBadge.setText ("TARGET: LOWER TONE [0x02]", juce::dontSendNotification);
        targetToneBadge.setColour (juce::Label::textColourId, juce::Colour (0xff3498db)); // Lower blue
    }
}

void PatchBrowserComponent::selectionChanged()
{
    if (folderTree)
    {
        if (auto* selectedItem = folderTree->getSelectedItem (0))
            selectedItem->setOpen (true);

        juce::File selectedFolderTreeItem = folderTree->getSelectedFile();
        if (selectedFolderTreeItem.exists() && selectedFolderTreeItem.isDirectory())
        {
            if (currentFolder != selectedFolderTreeItem)
            {
                currentFolder = selectedFolderTreeItem;
                pathLabel.setText ("CURRENT FOLDER:  " + currentFolder.getFullPathName(), juce::dontSendNotification);
                if (fileContentsList)
                    fileContentsList->setDirectory (currentFolder, false, true);
            }
        }
    }

    if (fileList)
    {
        juce::File f = fileList->getSelectedFile();
        if (f.existsAsFile() && (f.hasFileExtension (".syx") || f.hasFileExtension (".mid") || f.hasFileExtension (".midi")))
        {
            selectedFile = f;
        }
        else if (selectedFile.existsAsFile() && selectedFile.getParentDirectory() == currentFolder)
        {
            fileList->setSelectedFile (selectedFile);
        }
    }
}

void PatchBrowserComponent::fileClicked (const juce::File& file, const juce::MouseEvent&)
{
    if (file.isDirectory() && currentFolder != file)
    {
        if (folderTree)
        {
            if (auto* selectedItem = folderTree->getSelectedItem (0))
                selectedItem->setOpen (true);
        }
        currentFolder = file;
        pathLabel.setText ("CURRENT FOLDER:  " + currentFolder.getFullPathName(), juce::dontSendNotification);
        if (fileContentsList)
            fileContentsList->setDirectory (currentFolder, false, true);
    }
    else if (file.existsAsFile() && (file.hasFileExtension (".syx") || file.hasFileExtension (".mid") || file.hasFileExtension (".midi")))
    {
        selectedFile = file;
        if (fileList)
            fileList->setSelectedFile (selectedFile);
        handleLoadPatch();
    }
}

void PatchBrowserComponent::fileDoubleClicked (const juce::File& file)
{
    if (file.isDirectory())
    {
        currentFolder = file;
        pathLabel.setText ("CURRENT FOLDER:  " + currentFolder.getFullPathName(), juce::dontSendNotification);
        if (fileContentsList)
            fileContentsList->setDirectory (currentFolder, false, true);
    }
    else if (file.existsAsFile() && (file.hasFileExtension (".syx") || file.hasFileExtension (".mid") || file.hasFileExtension (".midi")))
    {
        selectedFile = file;
        if (fileList)
            fileList->setSelectedFile (selectedFile);
        handleLoadPatch();
    }
}

void PatchBrowserComponent::browserRootChanged (const juce::File&)
{
    // Required FileBrowserListener callback
}

void PatchBrowserComponent::refreshBrowser()
{
    SH201PatchSerializer::ensurePatchesDirectoryExists();
    if (folderContentsList)
        folderContentsList->refresh();
    if (fileContentsList)
        fileContentsList->refresh();
    if (folderTree)
        folderTree->refresh();
    statusLabel.setText ("Directory refreshed.", juce::dontSendNotification);
}

void PatchBrowserComponent::handleNewFolder()
{
    juce::String defaultName = "New Category";
    int counter = 1;
    while (currentFolder.getChildFile (defaultName).exists())
    {
        defaultName = "New Category " + juce::String (counter++);
    }

    auto* window = new juce::AlertWindow ("Create New Category Folder",
                                          "Enter name for the new folder:",
                                          juce::AlertWindow::NoIcon);
    window->addTextEditor ("folderName", defaultName);
    window->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    window->enterModalState (true, juce::ModalCallbackFunction::create ([this, window] (int result) {
        window->setVisible (false);
        if (result == 1)
        {
            juce::String name = window->getTextEditorContents ("folderName").trim();
            if (name.isNotEmpty())
            {
                juce::File newDir = currentFolder.getChildFile (name);
                if (! newDir.exists())
                {
                    if (newDir.createDirectory())
                    {
                        statusLabel.setText ("Created folder: " + name, juce::dontSendNotification);
                        if (folderContentsList)
                            folderContentsList->refresh();
                        if (fileContentsList)
                            fileContentsList->refresh();
                        if (folderTree)
                        {
                            folderTree->refresh();
                            folderTree->setSelectedFile (newDir);
                        }
                    }
                    else
                    {
                        statusLabel.setText ("Failed to create folder.", juce::dontSendNotification);
                    }
                }
                else
                {
                    statusLabel.setText ("Folder already exists.", juce::dontSendNotification);
                }
            }
        }
    }));
}

void PatchBrowserComponent::handleExportPatch()
{
    auto fileChooser = std::make_shared<juce::FileChooser> (
        "Export Current Loaded Patch As (.mid)",
        currentFolder.getChildFile ("New_Patch.mid"),
        "*.mid",
        true
    );

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, fileChooser] (const juce::FileChooser& chooser) {
            juce::File result = chooser.getResult();
            if (result != juce::File())
            {
                juce::File targetFile = result.hasFileExtension (".mid") ? result : result.withFileExtension (".mid");
                if (SH201PatchSerializer::exportPatchToFile (processor, targetFile))
                {
                    statusLabel.setText ("Exported patch: " + targetFile.getFileName(), juce::dontSendNotification);
                    refreshBrowser();
                }
                else
                {
                    statusLabel.setText ("Failed to export patch file.", juce::dontSendNotification);
                }
            }
        });
}

void PatchBrowserComponent::handleLoadPatch()
{
    if (! selectedFile.existsAsFile() || (! selectedFile.hasFileExtension (".syx") && ! selectedFile.hasFileExtension (".mid") && ! selectedFile.hasFileExtension (".midi")))
        return;

    int activePage = processor.getCurrentTonePage();
    if (SH201PatchSerializer::loadPatchIntoTone (processor, selectedFile, activePage))
    {
        juce::String toneName = (activePage == 0 ? "Upper" : "Lower");
        statusLabel.setText ("Loaded '" + selectedFile.getFileName() + "' into " + toneName + " Tone.", juce::dontSendNotification);
    }
    else
    {
        statusLabel.setText ("Failed to load tone data from file.", juce::dontSendNotification);
    }
}
