#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "PluginProcessor.h"
#include <vector>

class SH201PatchSerializer
{
public:
    static juce::File getPatchesRootDirectory();
    static bool ensurePatchesDirectoryExists();

    // Export current loaded patch to a .syx Roland SysEx file
    static bool exportPatchToFile (SH201AudioProcessor& processor, const juce::File& targetFile);

    // Load a .syx patch file into the currently selected tone page (0 = Upper, 1 = Lower)
    static bool loadPatchIntoTone (SH201AudioProcessor& processor, const juce::File& patchFile, int targetTonePage);

    // Utility for calculating Roland SysEx DT1 Checksum
    static uint8_t calculateRolandChecksum (const std::vector<uint8_t>& bytes, size_t addrStartIdx, size_t dataEndIdx);

    // Helpers to create Roland DT1 packets for saving/exporting
    static std::vector<uint8_t> createToneDT1Packet (SH201AudioProcessor& processor, const juce::String& tonePrefix, uint8_t toneAddr2);
    static std::vector<uint8_t> createCommonDT1Packet (SH201AudioProcessor& processor);
    static std::vector<uint8_t> createDelayDT1Packet (SH201AudioProcessor& processor);
    static std::vector<uint8_t> createReverbDT1Packet (SH201AudioProcessor& processor);
};
