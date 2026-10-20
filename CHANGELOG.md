# Roland SH-201 Editor VST - Changelog

All notable changes to this project will be documented in this file.

## [v1.2.1] - Hardware Knob Support in Stereo & Linked Modes + DAW Crash & MIDI Loop Fix
### Fixed
- **DAW Crash & MIDI Loop in Stereo Mode**: Resolved a critical issue where adjusting physical synthesizer knobs while Stereo Mode or panel parameter Link buttons were enabled crashed host DAWs (Ableton, FL Studio, Reaper, Cubase) and initiated recursive MIDI/SysEx feedback loops.
- **Thread-Safe Host Parameter Notification**: Eliminated unsafe calls to `param->setValueNotifyingHost` on background driver threads (`SH201_LibUSB_Thread` and MIDI input callback threads). All incoming hardware parameter updates are now cleanly dispatched to the JUCE message thread via `juce::MessageManager::callAsync`.
- **Libusb Re-entrancy & Deadlock Prevention**: Prohibited outgoing hardware SysEx transmission while actively processing incoming hardware edits (`isUpdatingFromHardware`), preventing synchronous `libusb_bulk_transfer` execution from within the libusb transfer completion callback.
- **SysEx Echo Suppression**: Implemented rigorous echo suppression in `handleIncomingSysEx` matching `handleIncomingMidiCC` using `lastHardwareTxTime` timestamps (500ms window), preventing hardware "Transmit Edit Data" SysEx replies from triggering infinite ping-pong feedback.
- **DAW Track MIDI Thru Echo Loop Elimination**: Filtered out synthesizer front-panel sound edit CC messages in `processBlock`, preventing DAW tracks with active MIDI monitoring from reflecting knob turn CCs back to the hardware.
- **Debounced Mirrored Hardware Synchronization**: Mirrored and linked parameter updates in Stereo Mode and panel Link modes are now smoothly debounced (120ms threshold after knob movement settles), updating the opposite hardware tone safely on the JUCE message thread without flooding the Roland SH-201 CPU or MIDI buffer.
- **UI Status Indication**: Added `[STEREO MIRROR]` status indication in the header bar's `LAST HW RX` readout when hardware knobs are turned while Stereo Mode is active.
- Updated VST header version subtitle to `EDITOR VST v1.2.1`.

## [v1.2.0] - Stable Release: DIN MIDI Data Routing & UX Fixes
### Fixed
- Fixed an issue where the VST was internally throttling DIN MIDI SysEx transmissions infinitely when knobs were turned, preventing parameter changes from ever reaching the synth.
- Fixed a false-positive connection status where the UI would say "CONNECTED" even if the DIN MIDI Output port failed to open (e.g., if the host DAW had locked it for exclusive use). The VST now correctly reports an error status if bidirectional communication cannot be established.
- Fixed a bug where the plugin would fail to reconnect to DIN MIDI ports if the hardware device's internal OS string identifier changed between reboots, by adding fallback name matching.
- Fixed occasional dropped real-time MIDI clock/note messages in DIN MIDI mode caused by an edge-case in the lock-free FIFO ring buffer wrap-around.
- Raised the hardware sync timeout in DIN MIDI mode to 2000ms to accommodate the slower 31.25 kbaud transfer rate of standard 5-pin MIDI compared to USB.
- Fixed Stereo Mode activation failing to push initialization SysEx packets in DIN MIDI mode by routing them correctly through the active manager instead of hardcoding USB.
- Removed constant XML disk I/O on the real-time audio thread, preventing potential audio dropouts and interface lag.
- Renamed the confusing "MIDI + Audio" and "MIDI only" Editor Mode options to "USB MIDI + Audio" and "USB MIDI Only" for clearer distinction from DIN MIDI mode.
- Updated VST header version subtitle to `EDITOR VST v1.2.0`.

## [v1.1.32] - DIN MIDI Hardware Control & Port Selection
### Added
- Added full **DIN MIDI** support as an option in Editor Mode (`System Settings -> Editor Mode -> DIN MIDI`), allowing users to control and bidirectionally edit the Roland SH-201 hardware using standard 5-pin DIN MIDI cables through any external MIDI interface, without requiring USB drivers.
- Added dynamic **MIDI In Port** and **MIDI Out Port** dropdown selectors to the System Settings window that appear when DIN MIDI mode is selected.
- Added persistent XML settings storage for `editor_mode`, `din_midi_in`, `din_midi_in_name`, `din_midi_out`, and `din_midi_out_name` in `SH201Editor.xml`.
- Created dedicated `SH201DinMidiManager` with a real-time lock-free output FIFO queue, background worker thread, automatic echo loop suppression, and graceful failure handling for disconnected/invalid MIDI ports.
- Updated VST header version subtitle to `EDITOR VST v1.1.32`.

## [v1.1.31] - Project Rebranding to Roland SH-201 Editor
### Changed
- Rebranded the plugin and standalone application from "Roland SH-201 Total Integration" to **Roland SH-201 Editor**.
- Standardized company name to **Transfict8 Audio Tools** across all metadata (`COMPANY_NAME`, `BUNDLE_ID` `com.Transfict8AudioTools.SH201Editor`, `GlobalSettings`, and NSIS installer).
- Updated GUI header subtitle to `EDITOR VST v1.1.31`.
- Updated NSIS installer script (`installer.nsi`) and CMake configurations (`CMakeLists.txt`) with new branding, target names, and artifact directory structures.
- Updated all documentation (`README.md`, `README.html`, `architecture.md`, `PROJECT_RULES.md`, `.cursorrules`) with full rebranding.
- Added backwards compatibility in `GlobalSettings` and `SH201PatchSerializer` to seamlessly access settings and patches from legacy directory locations.

## [v1.1.30] - Patch Loading and Stereo Mode Fixes
### Fixed
- Fixed an issue causing parameter jumps and patch export corruption when loading some third-party `.mid` patches. The VST now properly resolves conflicts when Common parameters (like Delay/Reverb levels) are duplicated with different values in the Upper and Lower tone blocks, and correctly handles pitch and tone balance encoding.
- Fixed a major MIDI feedback loop that caused crashes when adjusting hardware parameters (like filter cutoff) while Stereo Mode was active. The VST now uses a transmission lock to prevent parameter updates from echoing back to the hardware infinitely.
## [v1.1.29] - Standard MIDI File (.mid) Patch Export
### Added
- Changed Patch Importer/Exporter to natively support Standard MIDI Files (.mid) alongside legacy (.syx).
- Full backwards compatibility for loading previously exported SysEx banks.
- Users can now drag .mid patches directly into their DAW and send them to the hardware without opening the VST.

## [v1.1.28] - Editor Mode added to System Settings
### Added
- Added an "Editor Mode" dropdown to the System Settings window. This allows switching between "MIDI + Audio" (the default, streams audio and MIDI over USB) and "MIDI only" (turns off audio streaming to save USB bandwidth while maintaining MIDI control).

## [v1.1.27] - Tone Balance Control Inversion
### Changed
- Inverted the `common_tone_balance` parameter mapping. Turning the Tone Balance knob fully left now sets the Upper Tone to full volume, and turning it fully right sets the Lower Tone to full volume, which aligns better with the VST UI layout.
## [v1.1.26] - Pitch Knob Hardware Scaling Fix
### Fixed
- Fixed an issue where the Osc 1 and Osc 2 pitch knobs did not send the correct pitch scaling when Wide mode was OFF. The VST now properly multiplies the sent value by 3 and divides the received CC/SysEx values by 3 to ensure the knob always shifts the pitch a full +/- 12 semitones, accurately reflecting the hardware's expected parameter range.
## [v1.1.25] - Pitch Knob Dynamic Range for Wide Mode
### Fixed
- Fixed the Osc 1 and Osc 2 Pitch knobs so the full knob rotation maps correctly to the active semitone range. When Wide is OFF, the knob range is ±12 semitones; when Wide is ON, it expands to ±36 semitones. Previously, with Wide OFF the knob would stop having effect past ±12 while still visually turning through the full ±36 range.
- The pitch slider range now updates automatically when the Wide button is toggled, when a patch is pulled from hardware, or on startup.
## [v1.1.24] - Init Patch Button Auto-Pull Fix
### Fixed
- Fixed the Init Patch button not updating the VST interface on first press. The button now sends the init patch to hardware, waits for it to settle, then automatically pulls the full patch back (like PULL SYNTH) so all knobs and controls reflect the init state immediately. Added a safety timeout to prevent the UI from getting stuck in "SYNCING" state.
## [v1.1.23] - Stereo Mode and Mix/Mod Layout Tweaks
### Changed
- Shifted the Stereo Mode button and its Parameter Link dropdown slightly down and to the right for better visual symmetry with the Width knob.
- Nudged the Mix/Mod section dropdowns slightly to the right to better center them in their visual column.
## [v1.1.22] - Removed Bank Settings
### Changed
- Removed the unnecessary 'Bank MSB' and 'Bank LSB' knobs from the System Settings panel to declutter the interface.
## [v1.1.21] - Fix System Settings Slider Visibility
### Fixed
- Fixed an issue in the System Settings menu where the generic rotary sliders were not rendering correctly because they were mistakenly inheriting the custom look-and-feel logic meant for the main panel knobs.
## [v1.1.20] - Wide Button Refinements
### Changed
- Shortened the horizontal width of the Osc 1 and Osc 2 Wide buttons and right-aligned them within their layout cells so they nest closely to the visual connecting black lines.
## [v1.1.19] - Button Conversions & Layout Tweaks
### Changed
- Converted Osc 1/2 Wide and Amp Overdrive combobox dropdowns into latching text buttons.
- Lowered the LFO Rate knob (and its associated text) by several pixels for better visual centering.
## [v1.1.18] - LFO Fade Alignment
### Changed
- Shifted the Fade slider and its label down by several pixels so that the Fade text aligns perfectly horizontally with the Depth 1 and Depth 2 labels.
## [v1.1.17] - LFO Fine Tuning & Visual Balance
### Changed
- Shifted the Key Trigger button horizontally to center perfectly with the Rate knob in Column 2.
- Lowered the Depth 1 and Depth 2 knob clusters (and their corresponding Dest dropdowns) to center them vertically within the panel area.
- Shifted the Fade label down a few pixels so it sits tighter against the slider track and clearly clears the panel's UPPER TONE text.
## [v1.1.16] - LFO Panels Final Adjustments
### Changed
- Aligned the Key Trigger button perfectly inline with the Shape dropdown.
- Shifted the entire knob/value/label clusters downwards to improve spacing balance.
- Increased the vertical space between the knob titles and the tops of the knobs, giving the graphics more breathing room.
- Automatically repositioned Dest 1 and Dest 2 dropdowns to sit uniformly beneath the newly lowered depth clusters.
## [v1.1.15] - LFO Value Displays & Spacing
### Changed
- Shifted columns 3, 4, and 5 progressively further to the right to open up more horizontal breathing room between parameters.
- Re-enabled text value displays beneath the LFO Rate, Depth 1, and Depth 2 knobs, carefully balancing height and padding to fit nicely.
- Lowered the Dest 1 and Dest 2 dropdowns to make room for the newly visible text values under the depth knobs without bleeding over the panel bounds.
- Snugged the Rate, Depth 1, Depth 2, and Fade labels even tighter against the top edges of their respective controls.
## [v1.1.14] - LFO Panels Spacing & Alignment
### Changed
- Improved vertical spacing for LFO columns 1 and 2 to prevent text from impinging on the panel border.
- Reduced the width of the Sync and Key Trigger buttons to prevent horizontal overlap.
- Fine-tuned the vertical alignment of the LFO depth knobs and sliders, tightening the distance between their text labels and components.
- Shifted the Depth 2 (Column 4) parameters slightly to the right to provide adequate spacing from Depth 1.
## [v1.1.13] - LFO Panels Layout
### Changed
- Converted LFO Sync and Key Trigger parameters from Comboboxes to latching TextButtons (with glow effects).
- Converted LFO Fade Time from a rotary knob to a linear slider.
- Organized LFO 1 and LFO 2 layouts into exactly 5 evenly-spaced columns per user specifications.
- Enlarged LFO Rate, Depth 1, and Depth 2 knobs to the standard `68x84` size.
## [v1.1.12] - Master Effects Button Layout
### Changed
- Removed redundant external text labels for the Delay and Reverb buttons.
- Shifted the Reverb button upwards to clear the space above the ADVANCED button and improve vertical distribution.
## [v1.1.11] - Master Effects Buttons
### Changed
- Converted Delay and Reverb switches in the Master Effects panel from Comboboxes to Latching TextButtons.
- Added a red `juce::GlowEffect` to the Delay and Reverb buttons when toggled on.
- Scooted the ADVANCED button in the Master Effects panel up to `Y=180` to perfectly align horizontally with the Patch Common ADVANCED button.
## [v1.1.10] - Bottom Panel UI Layouts
### Changed
- Patch Common Panel: Enlarged the Tone Balance and Split Point knobs to 68x84 to match OSC knobs.
- Voice & Perform Panel: Reorganized the layout to stack the VOICE and PORTA dropdowns on the left, and enlarged the Time, Bend, and Octave knobs on the right.
- Master Effects Panel: Reorganized the layout to stack the DLY SW and REV SW above the ADVANCED button on the left, and enlarged the Delay and Reverb knobs on the right.
## [v1.1.9] - Filter Env Sliders
### Changed
- Converted Filter Env Depth control from a rotary knob to a linear slider to match the ADSR sliders.
- Globally increased the width of all linear ADSR sliders to 76 pixels.
## [v1.1.8] - Stereo Mode UI Tweak
### Changed
- Enlarged the Stereo Width knob to match the uniform OSC knob dimensions (68x84).
## [v1.1.7] - Pitch Envelope Sliders
### Changed
- Converted Pitch Envelope Attack and Decay controls from rotary knobs to vertical linear sliders to match the aesthetic of the Filter and Amp envelopes.
## [v1.1.6] - Mix / Mod UI Overhaul
### Changed
- Overhauled Mix / Mod panel layout: Split the area in half, vertically stacking the Type and Low Freq dropdowns on the left, and placing the enlarged Balance knob (68x84) vertically centered on the right.
## [v1.1.5] - Amp & Overdrive Tweaks
### Fixed
- Repositioned the Overdrive switch dropdown to the left so it sits cleanly above the Velocity Sens knob.
- Adjusted the visual linking line to span horizontally from the new position and correctly route to the Overdrive Level knob.
## [v1.1.4] - Amp & Overdrive UI Overhaul
### Changed
- Overhauled Amp & Overdrive panel layout to mirror Osc and Filter designs.
- Centered the Overdrive switch dropdown above the Overdrive Level knob.
- Drew a visual linking line connecting the Overdrive switch to the Overdrive Level knob.
- Enlarged Amp knobs to (68x84) and vertically centered them.
## [v1.1.3] - Filter UI Overhaul
### Changed
- Refactored the Filter panel to mirror the new Oscillator layouts: centered Type and Slope dropdowns at the top.
- Reordered bottom filter knobs to: Cutoff, Resonance, Keyfollow, Velocity Sens.
- Enlarged filter knobs to (68x84) and vertically centered the controls to prevent bottom overlap.
## [v1.1.2] - Pitch Line Tweak
### Fixed
- Adjusted the visual linking line connecting the 'Wide' dropdown to the 'Pitch' knob so it terminates correctly above the pitch label without intersecting the text, and reduced the line thickness for a cleaner look.
## [v1.1.1] - Pitch Wide Link Line
### Added
- Drew a visual linking line connecting the 'Wide' dropdown to the 'Pitch' knob in the Oscillator 1 and 2 panels to clarify their relationship.
## [v1.1.0] - Oscillator UI Overhaul
### Changed
- Overhauled Oscillator 1 and 2 panels: centered Wave and Wide dropdowns at the top.
- Moved Pitch knob down to sit horizontally inline with Detune, PW/Feedback, and Pitch Env Depth.
- Enlarged oscillator knobs (68x84) and their respective textboxes for better readability.
- Vertically aligned controls to clear the bottom bezel of the panels.
## [v1.0.28] - Brushed Metal UI Effect
### Added
- Implemented a procedural brushed metal texture background with a shiny gradient overlay for the silver UI panels (Oscillators, Filter, Amp, etc.) to mimic the hardware finish.
## [v1.0.27] - Consistent Panel Spacing
### Changed
- Adjusted the lower UI panel sections (LFO 1/2 and Amp & Overdrive) to strictly maintain a consistent 15px vertical spacing from the Envelope sections, resolving the uneven layout gap.
## [v1.0.26] - Last HW Rx Relocation
### Changed
- Moved the `LAST HW RX` label from the bottom footer to the top header, positioning it directly underneath the green USB connection status text.
## [v1.0.25] - Slider Dimensions
### Changed
- Increased width of linear envelope sliders by 15% (to 62px) to fix aspect ratio squishing.
- Increased height of envelope grid sections by 20% to allow for longer fader tracks, shifting lower panel sections down to accommodate.
## [v1.0.24] - Linear Slider Bugfix
### Fixed
- Envelopes (linear sliders) were missing the LookAndFeel assignment, causing them to fallback to JUCE default styling instead of using the custom `201slider.png` strip.
## [v1.0.23] - Slider Strip Rendering
### Changed
- Replaced the individual linear slider fader cap rendering with a complete, fully baked slider strip image (`201slider.png`).
## [v1.0.22] - Custom UI Adjustments
### Changed
- Scaled custom rotary knobs down by 10%.
- Reverted advanced menu knobs back to original vector graphic rendering.
- Fixed fader cap rendering for linear sliders by correcting JUCE drawImage int casting and widening component bounds.

## [v1.0.21] - Custom UI Knobs & Faders
### Added
- Updated the CustomLookAndFeel to use a hardware-accurate graphic knob strip for all rotary sliders, and a custom fader cap graphic for linear sliders.

## [v1.0.20] - Hardware Preset Change Sync Fix
### Fixed
- Fixed an issue where selecting a preset on the hardware would only update the Upper Tone in the VST UI. The manual patch memory request is now properly delayed to prevent collision with the hardware's automatic patch dump, ensuring the Lower Tone data is correctly fetched.

## [v1.0.19] - Init Patch Sync Hotfix
### Fixed
- Fixed an issue where the VST UI would not instantly reflect the hardware state on the very first click of the INIT PATCH button because the hardware's immediate SysEx dump was being ignored before the delayed fallback request triggered.

## [v1.0.18] - Company Metadata Update
### Changed
- Changed VST metadata `COMPANY_NAME` and `BUNDLE_ID` from 'Antigravity VST' to 'Trancsfict8' so the plugin displays the correct creator name in the DAW.

## [v1.0.17] - Independent Panel Parameter Linking
### Added
- Added an independent "LINK" latch button to the top-right corner of every sound design panel (OSC 1, OSC 2, MIX/MOD, PITCH ENV, FILTER, FILTER ENV, AMP ENV, AMP & OVERDRIVE, LFO 1, LFO 2).
- Turning a panel link ON instantly syncs that panel's parameters from the currently viewed tone to the opposite tone, and continuously mirrors any future tweaks you make inside that panel across both tones.
- Link states are saved as part of the DAW VST project state (they do not exist on the SH-201 hardware, so they are excluded from SysEx transmissions).

## [v1.0.16] - Global GUI Size Persistence
### Added
- Added a new 'GUI Size' dropdown menu in the SYSTEM SETTINGS panel.
### Changed
- The plugin now stores its GUI dimensions as a global application setting (`AntigravityVST/SH201TotalIntegration.xml`) rather than embedding the size into individual DAW project states. This ensures that the plugin always opens at your preferred size or most recently dragged size, completely ignoring whatever size might have been saved into a specific DAW project.

## [v1.0.15] - GUI Size Persistence Fix
### Fixed
- Fixed an issue where the VST GUI size would not persist correctly across DAW project loads. The editor size is now explicitly reapplied if the host loads the saved `AudioProcessorValueTreeState` state *after* instantiating the plugin editor window.

## [v1.0.14] - Init Patch Sync & UI Label Fixes
### Fixed
- Fixed the 'INIT PATCH' button so it now correctly triggers a hardware sync immediately after initializing the patch on the synthesizer, ensuring the VST UI properly reflects the initialized state without needing a manual "PULL SYNTH" click.
### Changed
- Updated UI knob labels in the Oscillator section to exactly match the physical SH-201 hardware labels:
  - Osc 1 'FINE' is now 'DETUNE'
  - Osc 1 'P.WIDTH' is now 'PW/FEEDBK'
  - Osc 2 'P.WIDTH' is now 'PW/FEEDBK'

## [v1.0.13] - Persistent GUI Size
### Added
- The VST GUI size is now automatically saved and remembered across sessions. When resizing the plugin window, the width and height are saved into the `AudioProcessorValueTreeState` and correctly restored upon next project load or plugin instantiation.

## [v1.0.12] - Block Unrequested Bulk Hardware Dumps
### Fixed
- Fixed an issue where the initial hardware patch dump (triggered automatically on VST load) could arrive *after* the DAW successfully loaded the project state, causing the VST to parse the delayed hardware dump and overwrite the user's saved envelopes (and other parameters) with the hardware's random preset. `handleIncomingSysEx` now explicitly ignores bulk SysEx dumps unless `isSyncingFromHardware` is actively true, guaranteeing the DAW project state remains fully authoritative.

## [v1.0.11] - Robust Project State Saving and Loading
### Fixed
- Fixed an issue where default parameters were not saved properly when writing to DAW project files due to missing APVTS initialisation.
- Added a 3000ms delay to hardware sync upon project load to allow the project to settle and avoid conflicts with initial hardware polling, preventing missed patches on slow project loads.

## [v1.0.10] - Skip Patch Selection Sync During Project Load
### Fixed
- Removed the transmission of Program Change / Patch selection parameters entirely during DAW project load. Sending these parameters caused the hardware to initiate a full preset recall from memory, which simultaneously echoed a SysEx patch dump back over USB to the VST. This unexpected incoming data was overwriting the VST's carefully restored state with the default preset, breaking the save/recall functionality. The plugin now skips these parameters and seamlessly pushes the customized project parameters directly into the hardware's edit buffer without triggering a preset load.

## [v1.0.9] - Fix Hardware State Override During Project Load
### Fixed
- Resolved an issue where loading a VST state would send the saved hardware Patch Program change *after* pushing the tone parameters, causing the hardware to overwrite the custom VST state with the original preset. Patch Program selection parameters are now sent first (Phase 1) with extra delay time, allowing the hardware to fully load the base preset before the remaining custom VST sound tweaks are applied (Phase 2).

## [v1.0.8] - Fix Missing Parameter Sync on Project Load
### Fixed
- Fixed an issue where the hardware synthesizer would drop some parameter updates when loading a saved DAW project. Parameter updates to the hardware are now rate-limited with a 10ms spacing interval to prevent overwhelming the synth's MIDI buffer during bulk project load.

## [v1.0.7] - Revert Hardware Latency Compensation
### Removed
- Reverted the DAW PDC parameter as the wrapper settings in DAW are preferred by the user.

## [v1.0.6] - Hardware Latency Compensation
### Added
- Added a new 'DAW PDC (ms)' parameter in the System Settings to automatically report hardware latency compensation to the DAW host.

## [v1.0.5] - UI Status Sync Fix
### Fixed
- Added a timeout fallback to `isSyncingFromHardware` so the UI status label doesn't get permanently stuck on 'SYNCING TO HARDWARE' if a SysEx chunk is dropped by the synth.
- Increased the pacing of `RQ1` requests to the hardware from 20ms to 40ms to prevent the hardware's MIDI buffer from overflowing.

## [v1.0.4] - Thread-Safe Patch Pull Fix
### Fixed
- Fixed an issue where pulling patch memory silently failed due to unsafe thread launching. Now correctly uses `juce::Timer::callAfterDelay` for hardware sync.
- Implemented auto-pull of System Common parameters (including Transmit PC) on hardware sync.

## [v1.0.3] - Auto-Sync on Hardware Preset Change
### Added
- Added automatic UI synchronization when a preset button is pressed on the hardware (triggered by MIDI Program Change).
- Mapped all remaining System Common SysEx parameters to allow correct hardware sync and persistence.
- Removed obsolete "PUSH PATCH" button from the UI.

## [v1.0.2] - Dynamic Audio Resampling
### Added
- Implemented real-time dynamic resampling using `juce::LagrangeInterpolator`.
- Plugin now correctly handles any DAW sample rate without audio stuttering or dropping USB packets from the 44.1kHz hardware stream.

## [v1.0.1] - UI Resizability and Layout Tweaks
### Changed
- Refactored dynamic UI layout to support global resizability with a fixed aspect ratio using affine transformations.
- Improved panel rendering with hardware-accurate geometric cutouts and heavy Impact typography.
- Re-distributed vertical spacing to eliminate parameter clipping inside panel boundaries.
- Clamped section panel tabs to perfectly respect underlying panel widths without overshooting.

## [v1.0.0] - Initial Version Baseline
### Added
- Complete bidirectional SysEx and MIDI CC integration with Roland SH-201 hardware.
- Advanced patch saving and loading system supporting `.sh201` file format.
- Automated hardware synchronization upon plugin instantiation without project state.
- Split UI tone pages for Upper and Lower synthesizer layers.
- Stereo mirroring mode linking parameters between tones.
- Meticulously mapped parameter CC routing for physical knob compatibility.
- Polished vector-based GUI with categorical styling and real-time syncing status indicators.
