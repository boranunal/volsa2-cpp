# VolSa 2 (C++ & Qt 6 Edition)

A high-performance C++20 translation and Qt 6 GUI sample manager for the **KORG Volca Sample 2** over ALSA MIDI Sequencer.

---

## Features

- **Accurate Protocol Implementation**:
  - Full SysEx encoding/decoding matching KORG's specification.
  - Bit-for-bit validated against actual hardware dump captures (`test_proto` validates against all 14 sample dumps).
  - 7-bit / 8-bit bidirectional packing and unpacking algorithm (`seven_bit.hpp`).
- **ALSA MIDI Sequencer Communication**:
  - Auto-discovery of `"volca sample"` device on Linux ALSA Sequencer.
  - Chunked transmission with configurable cooldown (default 10ms) to prevent device buffer overflows.
  - Handshake discovery (device echo, global channel, firmware version).
  - Space query (used/total sector capacity).
  - Slot querying (slots 0..199).
  - Upload, download, and deletion of samples.
- **Audio Processing**:
  - Sample conversion to 31.25 kHz mono PCM (Volca Sample 2 native format).
  - High-quality sinc resampling via `libsamplerate`.
  - Flexible mono downmixing modes:
    - **Mid**: `(Left + Right) / 2.0` (mono mix)
    - **Left**: Left channel only
    - **Right**: Right channel only
    - **Side**: `(Left - Right) / 2.0` (stereo difference)
  - Reads WAV, AIFF, FLAC, OGG, and other audio formats via `libsndfile`.
  - Exports standard 16-bit RIFF WAV files.
- **Qt 6 Desktop Application (`volsa2-gui`)**:
  - Modern, hardware-inspired dark theme UI.
  - Multi-threaded architecture (`DeviceWorker` on background `QThread`) ensuring a smooth, non-blocking UI during transfers.
  - Real-time device connection status and memory occupation bar.
  - Interactive table of all 200 sample slots with instant search & "Hide Empty" filters.
  - Custom interactive waveform visualizer (`WaveformWidget`) with real-time playback and seekbar.
  - Built-in audio audition player using Qt 6 Multimedia.
  - Drag-and-drop audio files directly onto sample slots.
  - Comprehensive Upload Dialog with mono mode selection, converted waveform auditioning, slot overwrite protection, and optional automatic backup.
  - Batch export ("Export All Occupied Samples...").
- **CLI Utility (`volsa2-cli`)**:
  - Command-line tool compatible with the original `volsa2` Rust CLI.

---

## Building

### Requirements
- C++20 compiler (`g++` >= 11 or `clang++` >= 13)
- CMake >= 3.20
- `alsa-lib` (`pkg-config alsa`)
- `libsamplerate` (`pkg-config samplerate`)
- `libsndfile` (`pkg-config sndfile`)
- Qt 6 (`Core`, `Gui`, `Widgets`, `Multimedia`)

### Build Commands
```sh
cd volsa2-cpp
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/local/Qt-6.9.1
cmake --build build -j$(nproc)
```

Run unit tests:
```sh
ctest --test-dir build --output-on-failure
```

---

## Usage

### 1. Qt GUI Application
Launch the GUI:
```sh
./build/gui/volsa2-gui
```
- Click **Connect** to detect and connect to your Volca Sample 2 over USB/ALSA MIDI.
- Click **Refresh All Slots** to scan slots 0..199.
- Click on any slot to load its waveform and play back audio.
- Drag any audio file onto a slot or click **Upload Sample...**.
- Right-click any row for contextual options (Play, Upload, Download, Erase).

### 2. Command Line (`volsa2-cli`)
```sh
# List loaded samples
./build/volsa2-cli list [-a]

# Download a sample slot to a WAV file
./build/volsa2-cli download 0 -o ./my_sample.wav

# Upload an audio file into the first available empty slot
./build/volsa2-cli upload drum.wav

# Upload into a specific slot with mono mode
./build/volsa2-cli upload drum.wav 5 -m mid

# Erase a slot
./build/volsa2-cli remove 5 -p
```
