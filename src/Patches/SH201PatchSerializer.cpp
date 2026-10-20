#include "SH201PatchSerializer.h"
#include <juce_audio_basics/juce_audio_basics.h>

juce::File SH201PatchSerializer::getPatchesRootDirectory()
{
    auto newDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                     .getChildFile ("Roland SH-201 Editor")
                     .getChildFile ("Patches");
    auto oldDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                     .getChildFile ("Roland SH201 Total Integration")
                     .getChildFile ("Patches");

    if (! newDir.exists())
        newDir.createDirectory();

    // Migrate any legacy patches/directories if they exist in the old folder
    if (oldDir.exists() && oldDir.isDirectory())
    {
        juce::Array<juce::File> files;
        oldDir.findChildFiles (files, juce::File::findFilesAndDirectories, true);
        for (const auto& file : files)
        {
            auto rel = file.getRelativePathFrom (oldDir);
            auto dest = newDir.getChildFile (rel);
            if (file.isDirectory())
            {
                if (! dest.exists())
                    dest.createDirectory();
            }
            else if (! dest.existsAsFile())
            {
                file.copyFileTo (dest);
            }
        }
    }

    return newDir;
}

bool SH201PatchSerializer::ensurePatchesDirectoryExists()
{
    juce::File rootDir = getPatchesRootDirectory();
    if (! rootDir.exists())
        return rootDir.createDirectory();
    return true;
}

uint8_t SH201PatchSerializer::calculateRolandChecksum (const std::vector<uint8_t>& bytes, size_t addrStartIdx, size_t dataEndIdx)
{
    int sum = 0;
    for (size_t i = addrStartIdx; i <= dataEndIdx; ++i)
        sum += bytes[i];
    return static_cast<uint8_t> ((128 - (sum % 128)) % 128);
}

std::vector<uint8_t> SH201PatchSerializer::createToneDT1Packet (SH201AudioProcessor& processor, const juce::String& tonePrefix, uint8_t toneAddr2)
{
    std::vector<uint8_t> packet;
    packet.push_back (0xF0);
    packet.push_back (0x41);
    packet.push_back (0x10); // Device ID
    packet.push_back (0x00);
    packet.push_back (0x00);
    packet.push_back (0x16); // SH-201 Model ID
    packet.push_back (0x12); // DT1 Command

    // Address: 10 00 01 00 (Upper) or 10 00 02 00 (Lower)
    packet.push_back (0x10);
    packet.push_back (0x00);
    packet.push_back (toneAddr2);
    packet.push_back (0x00);

    // 64 Data bytes (0x00 to 0x3F)
    for (uint8_t offset = 0; offset < 64; ++offset)
    {
        juce::String paramId;
        if      (offset == 0x25) paramId = "common_delay_level";
        else if (offset == 0x26) paramId = "common_reverb_level";
        else if (offset == 0x3B) paramId = "common_pitch_bend_range";
        else if (offset == 0x3C) paramId = "common_octave_shift";
        else if (offset == 0x3D) paramId = "common_portamento_switch";
        else if (offset == 0x3E) paramId = "common_portamento_time";
        else if (offset == 0x3F) paramId = "common_mono_solo_select";
        else
        {
            juce::String baseId = SH201AudioProcessor::getBaseParamIdByToneOffset (offset);
            if (baseId.isNotEmpty())
                paramId = tonePrefix + baseId;
        }

        uint8_t byteVal = 0;

        if (paramId.isNotEmpty())
        {
            if (auto* param = processor.getAPVTS().getParameter (paramId))
            {
                if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (param))
                {
                    byteVal = static_cast<uint8_t> (juce::jlimit (0, 127, choiceParam->getIndex()));
                }
                else if (auto* intParam = dynamic_cast<juce::AudioParameterInt*> (param))
                {
                    int unnormal = intParam->get();
                    if (paramId.endsWith ("filter_keyfollow"))
                        byteVal = static_cast<uint8_t> (juce::jlimit (0, 127, static_cast<int> (64 + std::round (unnormal / 10.0f))));
                    else if (intParam->getRange().getStart() < 0)
                    {
                        // Pitch params: convert from APVTS display range back to SysEx "wide" encoding
                        // when pitch_wide is OFF (must match /3 in handleIncomingSysEx decode)
                        if (paramId.endsWith ("osc1_pitch"))
                        {
                            auto* wideParam = processor.getAPVTS().getRawParameterValue (tonePrefix + "osc1_pitch_wide");
                            if (wideParam != nullptr && *wideParam <= 0.5f)
                                unnormal *= 3;
                        }
                        else if (paramId.endsWith ("osc2_pitch"))
                        {
                            auto* wideParam = processor.getAPVTS().getRawParameterValue (tonePrefix + "osc2_pitch_wide");
                            if (wideParam != nullptr && *wideParam <= 0.5f)
                                unnormal *= 3;
                        }
                        byteVal = static_cast<uint8_t> (juce::jlimit (0, 127, 64 + unnormal));
                    }
                    else
                        byteVal = static_cast<uint8_t> (juce::jlimit (0, 127, unnormal));
                }
            }
        }
        packet.push_back (byteVal);
    }

    uint8_t checksum = calculateRolandChecksum (packet, 7, packet.size() - 1);
    packet.push_back (checksum);
    packet.push_back (0xF7);

    return packet;
}

std::vector<uint8_t> SH201PatchSerializer::createCommonDT1Packet (SH201AudioProcessor& processor)
{
    std::vector<uint8_t> packet;
    packet.push_back (0xF0);
    packet.push_back (0x41);
    packet.push_back (0x10);
    packet.push_back (0x00);
    packet.push_back (0x00);
    packet.push_back (0x16);
    packet.push_back (0x12);

    // Address: 10 00 00 00 (Patch Common)
    packet.push_back (0x10);
    packet.push_back (0x00);
    packet.push_back (0x00);
    packet.push_back (0x00);

    // 33 Data bytes (0x00 to 0x20)
    for (uint8_t offset = 0; offset <= 0x20; ++offset)
    {
        uint8_t byteVal = 0;
        if (offset == 0x0C)
            byteVal = 127; // Default patch level (volume) to full so exported patches are never silent
        else if (offset == 0x0D)
            byteVal = 64;  // Default tone balance to center

        juce::String paramId = SH201AudioProcessor::getPatchCommonParamIdByOffset (offset);

        if (paramId.isNotEmpty())
        {
            if (auto* param = processor.getAPVTS().getParameter (paramId))
            {
                if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (param))
                {
                    byteVal = static_cast<uint8_t> (juce::jlimit (0, 127, choiceParam->getIndex()));
                }
                else if (auto* intParam = dynamic_cast<juce::AudioParameterInt*> (param))
                {
                    int unnormal = intParam->get();
                    if (intParam->getRange().getStart() < 0)
                    {
                        // Tone balance is negated in SysEx encoding (matches handleIncomingSysEx)
                        if (paramId == "common_tone_balance")
                            unnormal = -unnormal;
                        byteVal = static_cast<uint8_t> (juce::jlimit (0, 127, 64 + unnormal));
                    }
                    else
                        byteVal = static_cast<uint8_t> (juce::jlimit (0, 127, unnormal));
                }
            }
        }
        packet.push_back (byteVal);
    }

    uint8_t checksum = calculateRolandChecksum (packet, 7, packet.size() - 1);
    packet.push_back (checksum);
    packet.push_back (0xF7);
    return packet;
}

std::vector<uint8_t> SH201PatchSerializer::createDelayDT1Packet (SH201AudioProcessor& processor)
{
    std::vector<uint8_t> packet;
    packet.push_back (0xF0);
    packet.push_back (0x41);
    packet.push_back (0x10);
    packet.push_back (0x00);
    packet.push_back (0x00);
    packet.push_back (0x16);
    packet.push_back (0x12);

    // Address: 10 00 03 00 (Patch Delay)
    packet.push_back (0x10);
    packet.push_back (0x00);
    packet.push_back (0x03);
    packet.push_back (0x00);

    // 5 Data bytes (0x00 to 0x04)
    for (uint8_t offset = 0; offset < 5; ++offset)
    {
        uint8_t byteVal = 0;
        juce::String paramId;
        if      (offset == 0x00) paramId = "patch_delay_time";
        else if (offset == 0x01) paramId = "patch_delay_feedback";
        else if (offset == 0x02) paramId = "patch_delay_hf_damp";
        else if (offset == 0x03) paramId = "patch_delay_mod_rate";
        else if (offset == 0x04) paramId = "patch_delay_mod_depth";

        if (paramId.isNotEmpty())
        {
            if (auto* param = processor.getAPVTS().getParameter (paramId))
            {
                if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (param))
                {
                    byteVal = static_cast<uint8_t> (juce::jlimit (0, 127, choiceParam->getIndex()));
                }
                else if (auto* intParam = dynamic_cast<juce::AudioParameterInt*> (param))
                {
                    byteVal = static_cast<uint8_t> (juce::jlimit (0, 127, intParam->get()));
                }
            }
        }
        packet.push_back (byteVal);
    }

    uint8_t checksum = calculateRolandChecksum (packet, 7, packet.size() - 1);
    packet.push_back (checksum);
    packet.push_back (0xF7);

    return packet;
}

std::vector<uint8_t> SH201PatchSerializer::createReverbDT1Packet (SH201AudioProcessor& processor)
{
    std::vector<uint8_t> packet;
    packet.push_back (0xF0);
    packet.push_back (0x41);
    packet.push_back (0x10);
    packet.push_back (0x00);
    packet.push_back (0x00);
    packet.push_back (0x16);
    packet.push_back (0x12);

    // Address: 10 00 04 00 (Patch Reverb)
    packet.push_back (0x10);
    packet.push_back (0x00);
    packet.push_back (0x04);
    packet.push_back (0x00);

    // 10 Data bytes (0x00 to 0x09)
    for (uint8_t offset = 0; offset < 10; ++offset)
    {
        uint8_t byteVal = 0;
        juce::String paramId;
        if      (offset == 0x00) paramId = "patch_reverb_time";
        else if (offset == 0x01) paramId = "patch_reverb_pre_delay";
        else if (offset == 0x02) paramId = "patch_reverb_size";
        else if (offset == 0x03) paramId = "patch_reverb_high_cut";
        else if (offset == 0x04) paramId = "patch_reverb_density";
        else if (offset == 0x05) paramId = "patch_reverb_diffusion";
        else if (offset == 0x06) paramId = "patch_reverb_lf_damp_freq";
        else if (offset == 0x07) paramId = "patch_reverb_lf_damp_gain";
        else if (offset == 0x08) paramId = "patch_reverb_hf_damp_freq";
        else if (offset == 0x09) paramId = "patch_reverb_hf_damp_gain";

        if (paramId.isNotEmpty())
        {
            if (auto* param = processor.getAPVTS().getParameter (paramId))
            {
                if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (param))
                {
                    byteVal = static_cast<uint8_t> (juce::jlimit (0, 127, choiceParam->getIndex()));
                }
                else if (auto* intParam = dynamic_cast<juce::AudioParameterInt*> (param))
                {
                    byteVal = static_cast<uint8_t> (juce::jlimit (0, 127, intParam->get()));
                }
            }
        }
        packet.push_back (byteVal);
    }

    uint8_t checksum = calculateRolandChecksum (packet, 7, packet.size() - 1);
    packet.push_back (checksum);
    packet.push_back (0xF7);

    return packet;
}

bool SH201PatchSerializer::exportPatchToFile (SH201AudioProcessor& processor, const juce::File& targetFile)
{
    ensurePatchesDirectoryExists();

    // Automatically deactivate Stereo Mode before exporting so that the patch is saved
    // without stereo mode panned to one side or locked in DUAL keyboard mode.
    if (processor.isStereoModeActive())
    {
        juce::Logger::writeToLog ("SH201PatchSerializer: Stereo Mode active during patch export. Deactivating Stereo Mode to reset pans to center and keyboard mode to SINGLE before saving patch...");
        processor.deactivateStereoMode();
        if (auto* stereoParam = dynamic_cast<juce::RangedAudioParameter*> (processor.getAPVTS().getParameter ("common_stereo_mode")))
        {
            float targetNorm = stereoParam->convertTo0to1 (0.0f);
            stereoParam->setValueNotifyingHost (targetNorm);
            stereoParam->setValue (targetNorm);
        }
    }

    auto commonPacket = createCommonDT1Packet (processor);
    auto upperPacket  = createToneDT1Packet (processor, "upper_", 0x01);
    auto lowerPacket  = createToneDT1Packet (processor, "lower_", 0x02);
    auto delayPacket  = createDelayDT1Packet (processor);
    auto reverbPacket = createReverbDT1Packet (processor);

    if (targetFile.hasFileExtension("mid") || targetFile.hasFileExtension("midi"))
    {
        juce::MidiMessageSequence sequence;
        
        auto addSysex = [&sequence](const std::vector<uint8_t>& packet, double timestamp) {
            juce::MidiMessage msg (packet.data(), (int)packet.size());
            msg.setTimeStamp(timestamp);
            sequence.addEvent(msg);
        };
        
        addSysex (commonPacket, 0.0);
        addSysex (upperPacket,  0.5);
        addSysex (lowerPacket,  1.0);
        addSysex (delayPacket,  1.5);
        addSysex (reverbPacket, 2.0);
        
        sequence.updateMatchedPairs();
        
        juce::MidiFile midiFile;
        midiFile.setTicksPerQuarterNote(96);
        midiFile.addTrack(sequence);
        
        juce::FileOutputStream out(targetFile);
        if (!out.openedOk()) return false;
        out.setPosition(0);
        out.truncate();
        return midiFile.writeTo(out);
    }
    else
    {
        std::vector<uint8_t> allSyx;
        allSyx.insert (allSyx.end(), commonPacket.begin(), commonPacket.end());
        allSyx.insert (allSyx.end(), upperPacket.begin(), upperPacket.end());
        allSyx.insert (allSyx.end(), lowerPacket.begin(), lowerPacket.end());
        allSyx.insert (allSyx.end(), delayPacket.begin(), delayPacket.end());
        allSyx.insert (allSyx.end(), reverbPacket.begin(), reverbPacket.end());
    
        return targetFile.replaceWithData (allSyx.data(), allSyx.size());
    }
}

bool SH201PatchSerializer::loadPatchIntoTone (SH201AudioProcessor& processor, const juce::File& patchFile, int targetTonePage)
{
    if (! patchFile.existsAsFile())
        return false;

    std::vector<uint8_t> allSyxData;

    if (patchFile.hasFileExtension("mid") || patchFile.hasFileExtension("midi"))
    {
        juce::FileInputStream fis(patchFile);
        if (fis.openedOk())
        {
            juce::MidiFile midiFile;
            if (midiFile.readFrom(fis))
            {
                for (int t = 0; t < midiFile.getNumTracks(); ++t)
                {
                    const auto* track = midiFile.getTrack(t);
                    for (int i = 0; i < track->getNumEvents(); ++i)
                    {
                        const auto& msg = track->getEventPointer(i)->message;
                        if (msg.isSysEx())
                        {
                            const uint8_t* rawData = msg.getRawData();
                            int rawSize = msg.getRawDataSize();
                            allSyxData.insert(allSyxData.end(), rawData, rawData + rawSize);
                        }
                    }
                }
            }
        }
    }
    else
    {
        juce::MemoryBlock block;
        if (patchFile.loadFileAsData (block))
        {
            allSyxData.assign(static_cast<const uint8_t*>(block.getData()), static_cast<const uint8_t*>(block.getData()) + block.getSize());
        }
    }

    if (allSyxData.empty()) return false;

    const uint8_t* data = allSyxData.data();
    size_t size = allSyxData.size();

    bool toneLoaded = false;
    juce::String targetPrefix = (targetTonePage == 0 ? "upper_" : "lower_");
    uint8_t targetHardwareAddr2 = (targetTonePage == 0 ? 0x01 : 0x02);

    // Collect hardware DT1 packets to send paced after parameter updates
    std::vector<std::vector<uint8_t>> hwPacketsToSend;

    // Phase 3 fix: Suppress individual per-parameter DT1 SysEx echo during patch load.
    // Only the paced bulk DT1 packets at the end should be sent to hardware.
    processor.setSuppressHardwareEcho(true);

    // Scan through sysex data for F0 41 ... F7 DT1 packets
    size_t i = 0;
    while (i + 14 <= size)
    {
        if (data[i] == 0xF0 && data[i + 1] == 0x41 && data[i + 5] == 0x16 && data[i + 6] == 0x12)
        {
            uint8_t addr0 = data[i + 7];
            uint8_t addr2 = data[i + 9];
            uint8_t addr3 = data[i + 10];

            // Find end of packet (0xF7)
            size_t endIdx = i + 7;
            while (endIdx < size && data[endIdx] != 0xF7)
                ++endIdx;

            if (endIdx < size && data[endIdx] == 0xF7)
            {
                // Check if this packet is Patch Common, Tone memory, Delay, or Reverb (addr2 <= 0x04)
                if ((addr0 == 0x10 || addr0 == 0x20) && (addr2 <= 0x04))
                {
                    size_t numDataBytes = (endIdx - 1) - (i + 11);
                    std::vector<uint8_t> hwData;
                    // Ensure Upper tone data stays in Upper and Lower tone data stays in Lower,
                    // otherwise full-patch dumps will overwrite the target tone with the Lower tone (usually Init).
                    uint8_t targetHwAddr2 = addr2;
                    juce::String tonePrefix = (addr2 == 0x02 ? "lower_" : "upper_");

                    for (size_t d = 0; d < numDataBytes; ++d)
                    {
                        uint8_t offset = static_cast<uint8_t> (addr3 + d);
                        uint8_t val = data[i + 11 + d];
                        hwData.push_back (val);

                        juce::String paramId;
                        if (addr2 == 0x00) // Patch Common
                        {
                            paramId = SH201AudioProcessor::getPatchCommonParamIdByOffset (offset);
                        }
                        else if (addr2 == 0x01 || addr2 == 0x02) // Tone Memory
                        {
                            // Common params embedded in tone blocks (delay/reverb levels, pitch bend range,
                            // octave shift, portamento, mono/solo) exist in BOTH the Upper and Lower tone
                            // SysEx packets but may contain DIFFERENT values in third-party .mid patches.
                            // The SH-201 hardware treats the Upper tone's copies as authoritative, so we
                            // must only load these from the Upper tone block (addr2 == 0x01).
                            // Loading from both causes the Lower tone's stale values to overwrite
                            // the correct Upper values, leading to parameter jumps on "Pull Synth"
                            // and round-trip corruption when re-exporting the patch.
                            if (offset == 0x25 || offset == 0x26 ||
                                (offset >= 0x3B && offset <= 0x3F))
                            {
                                if (addr2 == 0x01) // Only load common-in-tone params from Upper
                                {
                                    if      (offset == 0x25) paramId = "common_delay_level";
                                    else if (offset == 0x26) paramId = "common_reverb_level";
                                    else if (offset == 0x3B) paramId = "common_pitch_bend_range";
                                    else if (offset == 0x3C) paramId = "common_octave_shift";
                                    else if (offset == 0x3D) paramId = "common_portamento_switch";
                                    else if (offset == 0x3E) paramId = "common_portamento_time";
                                    else if (offset == 0x3F) paramId = "common_mono_solo_select";
                                }
                                // else: addr2 == 0x02 (Lower) — skip these common offsets
                            }
                            else
                            {
                                juce::String baseId = SH201AudioProcessor::getBaseParamIdByToneOffset (offset);
                                if (baseId.isNotEmpty())
                                    paramId = tonePrefix + baseId;
                            }
                        }
                        else if (addr2 == 0x03) // Patch Delay
                        {
                            if      (offset == 0x00) paramId = "patch_delay_time";
                            else if (offset == 0x01) paramId = "patch_delay_feedback";
                            else if (offset == 0x02) paramId = "patch_delay_hf_damp";
                            else if (offset == 0x03) paramId = "patch_delay_mod_rate";
                            else if (offset == 0x04) paramId = "patch_delay_mod_depth";
                        }
                        else if (addr2 == 0x04) // Patch Reverb
                        {
                            if      (offset == 0x00) paramId = "patch_reverb_time";
                            else if (offset == 0x01) paramId = "patch_reverb_pre_delay";
                            else if (offset == 0x02) paramId = "patch_reverb_size";
                            else if (offset == 0x03) paramId = "patch_reverb_high_cut";
                            else if (offset == 0x04) paramId = "patch_reverb_density";
                            else if (offset == 0x05) paramId = "patch_reverb_diffusion";
                            else if (offset == 0x06) paramId = "patch_reverb_lf_damp_freq";
                            else if (offset == 0x07) paramId = "patch_reverb_lf_damp_gain";
                            else if (offset == 0x08) paramId = "patch_reverb_hf_damp_freq";
                            else if (offset == 0x09) paramId = "patch_reverb_hf_damp_gain";
                        }

                        if (paramId.isNotEmpty())
                        {
                            if (auto* param = processor.getAPVTS().getParameter (paramId))
                            {
                                float normVal = 0.0f;
                                if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (param))
                                {
                                    int numChoices = choiceParam->choices.size();
                                    normVal = juce::jlimit (0, numChoices - 1, static_cast<int> (val)) / static_cast<float> (juce::jmax (1, numChoices - 1));
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
                                        // Tone balance is negated in SysEx encoding
                                        if (paramId == "common_tone_balance")
                                            decodedInt = -decodedInt;
                                        // Pitch params: convert from SysEx "wide" encoding to APVTS display range
                                        // when pitch_wide is OFF (must match handleIncomingSysEx)
                                        if (paramId.endsWith ("osc1_pitch"))
                                        {
                                            auto* wideParam = processor.getAPVTS().getRawParameterValue (tonePrefix + "osc1_pitch_wide");
                                            if (wideParam != nullptr && *wideParam <= 0.5f)
                                                decodedInt /= 3;
                                        }
                                        else if (paramId.endsWith ("osc2_pitch"))
                                        {
                                            auto* wideParam = processor.getAPVTS().getRawParameterValue (tonePrefix + "osc2_pitch_wide");
                                            if (wideParam != nullptr && *wideParam <= 0.5f)
                                                decodedInt /= 3;
                                        }
                                    }

                                    normVal = intParam->convertTo0to1 (juce::jlimit (minVal, maxVal, decodedInt));
                                }
                                processor.markParameterUpdatedFromHardware (paramId);
                                param->setValueNotifyingHost (normVal);
                            }
                        }
                    }

                    // Collect DT1 packet for paced hardware send after parameter updates
                    if (! hwData.empty())
                    {
                        std::vector<uint8_t> hwPacket;
                        hwPacket.push_back (0xF0);
                        hwPacket.push_back (0x41);
                        hwPacket.push_back (0x10);
                        hwPacket.push_back (0x00);
                        hwPacket.push_back (0x00);
                        hwPacket.push_back (0x16);
                        hwPacket.push_back (0x12);
                        hwPacket.push_back (0x10);
                        hwPacket.push_back (0x00);
                        hwPacket.push_back (targetHwAddr2);
                        hwPacket.push_back (addr3);
                        hwPacket.insert (hwPacket.end(), hwData.begin(), hwData.end());
                        uint8_t chk = calculateRolandChecksum (hwPacket, 7, hwPacket.size() - 1);
                        hwPacket.push_back (chk);
                        hwPacket.push_back (0xF7);

                        hwPacketsToSend.push_back (std::move (hwPacket));
                    }

                    toneLoaded = true;
                }
                i = endIdx + 1;
                continue;
            }
        }
        ++i;
    }

    // Phase 3 fix: Re-enable hardware echo now that parameter updates are complete
    processor.setSuppressHardwareEcho(false);

    // Send collected DT1 packets to hardware with 30ms pacing to prevent
    // overwhelming the SH-201's DSP (which kills audio output).
    if (! hwPacketsToSend.empty())
    {
        for (size_t idx = 0; idx < hwPacketsToSend.size(); ++idx)
        {
            juce::Timer::callAfterDelay (static_cast<int> (30 * idx), [packet = hwPacketsToSend[idx], &processor]() {
                processor.sendSysExToHardware (packet);
            });
        }
    }

    return toneLoaded;
}
