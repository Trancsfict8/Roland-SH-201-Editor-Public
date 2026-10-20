# Roland SH-201 Editor

![Roland SH-201 Editor](readme%20pics/Main%20Plugin%20UI%20Screenshot.png)

![Roland SH-201](https://img.shields.io/badge/Roland-SH--201-00e5ff?style=for-the-badge&labelColor=11141a)
![VST3](https://img.shields.io/badge/Plugin-VST3%20%7C%20Standalone-00e5ff?style=for-the-badge&labelColor=11141a)
![JUCE 8](https://img.shields.io/badge/Framework-JUCE%208-00e5ff?style=for-the-badge&labelColor=11141a)
![License](https://img.shields.io/badge/License-MIT-00e5ff?style=for-the-badge&labelColor=11141a)

A 64-bit **VST3 Plugin & Standalone Controller** for the **Roland SH-201 Hardware Synthesizer**, developed by **Transfict8 Audio Tools** to bring deep hardware control, complete session recall, and USB digital audio streaming to modern operating systems and DAWs.

---

## Installation & Setup

1. **Run the Installer**:
   Download and execute the latest installer executable (`Roland_SH-201_Editor_Installer_vX.X.X.exe`). This installs the 64-bit VST3 plugin to your system's standard VST3 directory, the Standalone application, and documentation.

2. **Choose Your Connection Mode**:
   - **USB Connection (Audio + MIDI / MIDI Only)**: Connect the SH-201 to your computer via USB. Because official Roland 64-bit drivers are unavailable for modern Windows, a WinUSB driver is required. Please consult the documentation packaged with the installer (`HOW TO INSTALL DRIVER PLEASE READ.pdf`) for instructions on installing the driver using the bundled Zadig tool.
   - **DIN MIDI Connection (No Driver Required)**: You can connect the SH-201 directly through any standard MIDI interface using standard 5-pin DIN MIDI cables. Open **System Settings** in the plugin header, select **DIN MIDI** as the Editor Mode, and choose your hardware MIDI In and MIDI Out ports. No custom USB driver installation is required.

---

## Overview & Features

The Roland SH-201 is a classic hands-on virtual-analog synthesizer, but lacks 64-bit software integration for modern DAWs. This editor bridges the gap with a high-performance control surface and digital audio interface:

- **Hardware-Matched Interface**: The user interface directly reflects the layout, sections, and controls of the physical SH-201 front panel (Osc 1, Osc 2, Mix/Mod, Filter, Envelopes, LFOs, and Master Effects), providing a familiar workflow for hardware owners.
- **111-Parameter Control Surface**: Full bidirectional control across Upper Tone (48 parameters), Lower Tone (48 parameters), and Patch Common / Voice / Master Effects (15 parameters).
- **DAW State Recall & Automation**: All parameter values are saved directly inside your DAW project file (`AudioProcessorValueTreeState`). Opening a saved session automatically restores and synchronizes the patch to the hardware synthesizer without audio dropouts.
- **Bidirectional Hardware Synchronization**: Turning physical knobs on the synthesizer updates the plugin sliders in real time, and adjusting controls in the plugin immediately updates the hardware sound engine.
- **USB Digital Audio Streaming**: Captures 24-bit stereo PCM audio directly from the SH-201's USB connection using a lock-free FIFO queue, with real-time sample rate conversion supporting any DAW sample rate (44.1 kHz, 48 kHz, 96 kHz, etc.).
- **Stereo Mode & Parameter Linking**: Includes a dedicated Stereo Mode that runs the synthesizer in Dual mode with wide L/R stereo panning, as well as individual red "LINK" buttons on every panel to mirror parameter adjustments across Upper and Lower tones in real time.
- **Preset Management**: Built-in Patch Browser supporting both Standard MIDI Files (`.mid`) and SysEx (`.syx`) patch banks for easy preset import, export, and organization.
- **Advanced System Settings**: Easy access to global settings including Master Tune, Transpose, System Tempo, MIDI routing, and D-Beam parameters via the System Settings window.

---

## Parameter Mapping Architecture

All parameters map directly to authoritative **Roland SH-201 SysEx Patch Memory (`0x10 0x00 00/01/02 xx`)**:

### 1. Static Patch Common & Master Parameters (`10 00 00 xx`) - Always Visible
| Section | Parameter | VST APVTS ID | SysEx Offset | Values / Range |
| :--- | :--- | :--- | :--- | :--- |
| **Patch Common** | Keyboard Mode | `common_keyboard_mode` | `0x00` | SINGLE, DUAL, SPLIT |
| | Tone Balance | `common_tone_balance` | `0x01` | 0 to 127 (64 = Center) |
| | Split Point | `common_split_point` | `0x02` | 0 to 127 (C4 = 60) |
| **Stereo Mode (VST)** | Stereo Mode Switch | `common_stereo_mode` | VST Host / Mirror | 0=OFF, 1=ON |
| | Stereo Width | `common_stereo_width` | VST Host / Pan Control | 0 to 100% |
| | Param Link Mode | `common_stereo_link_mode` | VST Host / Mirror | LINK ALL, EXCLUDE OSC |
| **Voice / Perform** | Mono/Solo Mode | `common_mono_solo_select` | `0x04` | POLY, SOLO+L, SOLO |
| | Portamento Switch | `common_portamento_switch` | `0x05` | 0=OFF, 1=ON |
| | Portamento Time | `common_portamento_time` | `0x06` | 0 to 127 |
| | Pitch Bend Range | `common_pitch_bend_range` | `0x07` | 0 to 24 Semitones |
| | Octave Shift | `common_octave_shift` | `0x08` | -3 to +3 Octaves |
| **Master Effects** | Delay Switch & Level | `common_delay_switch` / `common_delay_level` | `0x15` / `0x16` | OFF/ON, 0 to 127 |
| | Reverb Switch & Level | `common_reverb_switch` / `common_reverb_level` | `0x1E` / `0x1F` | OFF/ON, 0 to 127 |

### 2. Dual Tone Layers: Upper Tone (`10 00 01 xx`) & Lower Tone (`10 00 02 xx`)
Each tone layer features 48 parameters prefixed with `upper_` or `lower_`:
| Section | Parameter | VST APVTS ID (`upper_` / `lower_`) | SysEx Offset | MIDI CCs | Values / Range |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Oscillator 1** | Waveform | `*_osc1_wave` | `0x00` | CC# 70 / CC# 100 | Saw, Square, PW-Sqr, Tri, Sine, Noise, Feedback, SuperSaw, Ext |
| | Pitch | `*_osc1_pitch` | `0x02` | CC# 14 | -36 to +36 Semitones |
| | Pulse Width | `*_osc1_pulse_width` | `0x04` | CC# 16 | 0 to 127 |
| **Oscillator 2** | Waveform | `*_osc2_wave` | `0x06` | CC# 81 / CC# 101 | Saw, Square, PW-Sqr, Tri, Sine, Noise, Feedback, SuperSaw, Ext |
| | Pitch | `*_osc2_pitch` | `0x08` | CC# 15 | -36 to +36 Semitones |
| | Detune | `*_osc2_detune` | `0x09` | CC# 17 | -50 to +50 Cents (0 to 127) |
| **Mix / Mod** | Mod Type | `*_mix_mod_type` | `0x0D` | — | MIX, SYNC, RING |
| | Balance | `*_mix_mod_balance` | `0x0F` | CC# 22 | 0 (OSC1) to 127 (OSC2) |
| **Filter** | Type | `*_filter_type` | `0x11` | CC# 105 | BYPASS, LPF, HPF, BPF |
| | Cutoff | `*_filter_cutoff` | `0x13` | CC# 74 / CC# 102 | 0 to 127 |
| | Resonance | `*_filter_resonance` | `0x16` | CC# 71 / CC# 104 | 0 to 127 |
| **Envelopes** | Filter ADSR | `*_filter_attack` .. `*_filter_release` | `0x17` .. `0x1A` | CC# 76, 77, 78, 80 | 0 to 127 |
| | Amp ADSR | `*_amp_attack` .. `*_amp_release` | `0x21` .. `0x24` | CC# 73, 75, 79, 72 | 0 to 127 |
| **LFO 1 & 2** | Shape / Rate / Sync | `*_lfo1_shape`, `*_lfo1_rate`, `*_lfo1_tempo_sync` | `0x26` .. `0x2A` | CC# 18 / 114 | TRI, SIN, SAW, SQR, TRP, S&H, RND (Sync 0=ON, 1=OFF) |
| **Amp & Drive** | Overdrive Level | `*_overdrive_level` | `0x1D` | CC# 12 / CC# 118 | 0 to 127 |

---

## Dependencies & Requirements

* **Operating System**: Windows 10 / 11 (64-bit)
* **Compiler / IDE**: Visual Studio 2022 (MSVC x64 toolset with C++20 support)
* **Build System**: CMake 3.22 or higher
* **JUCE Framework**: JUCE 8 (`v8.0.15`, automatically fetched by CMake via `FetchContent`)
* **USB Driver & Library**: `libusb-1.0` (Pre-bundled in `third_party/libusb/`)

---

## Building from Source

### 1. Clone the Repository
```powershell
git clone https://github.com/Trancsfict8/Roland-SH-201-Total-Integration.git
cd Roland-SH-201-Total-Integration
```

### 2. Configure with CMake
Generate the Visual Studio x64 solution inside the `build/` directory:
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

### 3. Build in Release Mode
Compile both the Standalone executable and the VST3 plugin bundle:
```powershell
cmake --build build --config Release
```

### 4. Locate Compiled Binaries
Output artifacts are generated in:
* **VST3 Plugin**: `build/SH201Editor_artefacts/Release/VST3/Roland SH-201 Editor.vst3`
* **Standalone App**: `build/SH201Editor_artefacts/Release/Standalone/Roland SH-201 Editor.exe`

---

## Project Structure

```
Roland-SH-201-Editor/
├── CMakeLists.txt                         # CMake build script & JUCE FetchContent configuration
├── architecture.md                        # Technical VST3/USB architecture & design roadmap
├── CHANGELOG.md                           # Version changelog
├── LICENSE                                # MIT License
├── src/
│   ├── PluginProcessor.h / .cpp           # Audio processing, APVTS state, SysEx & MIDI routing
│   ├── PluginEditor.h / .cpp              # Hardware-matched UI & custom rotary controls
│   ├── MIDI/
│   │   └── SH201DinMidiManager.h / .cpp   # DIN MIDI interface, lock-free queue & device handling
│   ├── Patches/
│   │   ├── SH201PatchSerializer.h / .cpp  # Patch file import/export (.mid and .syx)
│   │   └── PatchBrowserComponent.h / .cpp # Preset manager and patch browser overlay
│   └── USB/
│       └── SH201USBManager.h / .cpp       # Dedicated libusb thread, Isochronous IN PCM & Bulk I/O
├── third_party/
│   └── libusb/                            # Pre-bundled libusb-1.0 headers, import libs, and DLL
└── tools/
    ├── build.ps1                          # Helper script for one-click build automation
    └── *.py                               # USB/MIDI diagnostic and live-knob testing scripts
```

---

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

Roland and SH-201 are registered trademarks of Roland Corporation. This software is an independent open-source project created by Transfict8 Audio Tools and is not affiliated with or endorsed by Roland Corporation.
