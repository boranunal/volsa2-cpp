# Technical Specification & System Architecture: VolSa 2 (C++ & Qt 6 Edition)

**Project Name:** VolSa 2  
**Target Platform:** Linux (ALSA MIDI Sequencer & ALSA PCM Audio Subsystems)  
**Language Standards:** Modern C++ (C++20), Qt 6.9+  
**Target Hardware:** KORG Volca Sample 2 Digital Sample Sequencer (USB MIDI Class Compliant)  
**Document Revision:** 2.0.0 (Comprehensive Engineering & Architectural Reference)  
**Document Classification:** Exhaustive Technical Manual & From-Scratch Implementation Guide  

---

## Executive Overview & Table of Contents

This document serves as the definitive technical manual, protocol specification, and architectural guide for **VolSa 2**, an open-source, deterministic Linux librarian and digital audio processing engine for the **KORG Volca Sample 2**.

Written for systems programmers, embedded audio engineers, and digital signal processing specialists, this document explains every mathematical principle, protocol transaction, kernel audio subsystem interaction, and software design pattern required to build this entire system from absolute scratch.

```
===================================================================================
                               TABLE OF CONTENTS
===================================================================================
1.  EXECUTIVE SUMMARY & HARDWARE SPECIFICATIONS
    1.1 Hardware Lineage: Volca Sample 1 vs. Volca Sample 2
    1.2 Electrical, USB & Microcontroller Architecture
    1.3 Internal Flash Memory Organization & Sector Allocation
    1.4 Native Audio Hardware Architecture (31,250 Hz 16-bit Mono PCM)
    1.5 Architectural Goals & Constraints of VolSa 2

2.  THE COMPLETE FROM-SCRATCH ENGINEERING BLUEPRINT
    2.1 Mental Model & System Topology
    2.2 Ten-Stage Step-by-Step Implementation Roadmap
    2.3 End-to-End Upload Dataflow: WAV on Disk to Volca Flash
    2.4 End-to-End Download Dataflow: Volca Flash to RIFF WAV File
    2.5 Layered Component Architecture & Thread Boundaries

3.  THE LINUX AUDIO INFRASTRUCTURE: COMPREHENSIVE ALSA INTERFACE
    3.1 Architecture of the Linux Sound Subsystem
    3.2 The ALSA MIDI Sequencer Subsystem (`snd_seq_*`)
        3.2.1 Why Sequencer API vs. RawMIDI (`/dev/snd/midiC*D*`)
        3.2.2 Client Lifecycle, Port Creation, and Duplex Capabilities
        3.2.3 Dynamic Client Enumeration & Automated Hardware Discovery
        3.2.4 Subscription Management & Routing Topology
        3.2.5 Event Architecture (`snd_seq_event_t`) & Direct Dispatch
        3.2.6 Kernel Event Pool & Buffer Sizing Mechanics
        3.2.7 Non-Blocking Asynchronous Event Polling
        3.2.8 Draining, Purging, and Active Sensing Flood Rejection
    3.3 The ALSA PCM Audio Subsystem (`snd_pcm_*`)
        3.3.1 PCM Fundamentals: Frames, Periods, and Ring Buffers
        3.3.2 Hardware Configuration & Parameter Negotiation
        3.3.3 The 31,250 Hz Mono Playback Challenge & Stereo Duplication Fallback
        3.3.4 Audio Buffer Underrun (XRUN) Recovery Patterns
        3.3.5 Embedded Threaded Audio Engine (`AlsaAudioPlayer`)

4.  KORG MIDI SYSTEM EXCLUSIVE (SysEx) PROTOCOL SPECIFICATION
    4.1 MIDI 1.0 System Exclusive Framing Standards
    4.2 KORG Manufacturer Framing: Standard vs. Extended Headers
    4.3 Global MIDI Channel Addressing & Math
    4.4 Device Discovery & Handshake Protocol (`SearchDeviceRequest` / `Reply`)
    4.5 Sector Memory Query Protocol (`SampleSpaceDumpRequest` / `Reply`)
    4.6 Sample Metadata Header Specification (`SampleHeaderDumpRequest` / `SampleHeader`)
        4.6.1 32-Byte Unpacked Memory Layout
        4.6.2 Slot Erasure & Vacancy Conventions
    4.7 Raw PCM Sample Data Protocol (`SampleDataDumpRequest` / `SampleData`)
    4.8 Status Acknowledgment & NAK Protocol (`ACK_STATUS` / `NakStatus`)
    4.9 Hardware Flow Control & USB Packet Slicing
    4.10 Hardware Sequencer Pattern Protocol (`PatternData`, Function 0x4D)
        4.10.1 9,079-Byte SysEx Message Framing & 7,936-Byte Unpacked Binary
        4.10.2 Pattern Memory Layout: 'PTST' Magic, Name Offset 16, and Step Sequences
        4.10.3 In-Place Pattern Renaming Algorithm Preserving Trigger & Motion Automation

5.  MATHEMATICAL & ALGORITHMIC FOUNDATIONS: 7-BIT MIDI OCTET PACKING
    5.1 The 7-Bit Invariant: The Status Byte Collision Problem
    5.2 Mathematical Formulation of 7-to-8 Octet Packing
    5.3 Step-by-Step Bitwise Assembly Algorithm (`U8ToU7`)
    5.4 Inverse Unpacking & Bit Restoration Algorithm (`U7ToU8`)
    5.5 Exact Mathematical Buffer Length Transformation Formulas
    5.6 Mathematical Boundary Invariants: The $8n + 1$ Invalidation Law

6.  DIGITAL SIGNAL PROCESSING (DSP) & AUDIO TRANSFORMATION ENGINE
    6.1 Ingestion & Multi-Codec Audio Decoding (`libsndfile`)
    6.2 Multi-Channel Downmixing Matrices (Mid, Left, Right, Side)
    6.3 Band-Limited Sinc Polyphase Resampling (`libsamplerate`)
        6.3.1 Mathematical Principles of Band-Limited Interpolation
        6.3.2 Nyquist Filter Design & Anti-Aliasing Cutoff
    6.4 Clamping, Dynamic Range Scaling, and Quantization
    6.5 RIFF/WAVE Format Serialization Engine (44-Byte Container)
    6.6 Audio Slicing & Transient Chopper DSP Engine (`volsa2::SampleChopper`)
        6.6.1 Short-Time Energy & Spectral Flux Transient Detection
        6.6.2 Equal-Division & Beat/Grid Subdivision Models
        6.6.3 Zero-Crossing Snapping Algorithm to Eliminate Boundary Artifacts
        6.6.4 Noise-Floor Envelope Silence Auto-Trimming
        6.6.5 Slice Target Slot Management & Auto-Advancing Allocation

7.  KORG PRESET PACKAGE ARCHIVE SPECIFICATION (`.ivlcsplpreset`)
    7.1 Official KORG ZIP Container Hierarchy & File Topology
    7.2 JSON Manifest Metadata Schema (`preset.json` / `info.json`)
    7.3 Program Sequencer Archival (`programs/`)
    7.4 Sample Descriptors & 31,250 Hz PCM Serialization (`samples/`)
    7.5 The Flash Sector Exhaustion Problem: Allocation Mechanics & Sector Reclaim
    7.6 The Pre-Clean Reclaim Algorithm (Phase 1 Flash Cleaning Prior to Audio Transmission)

8.  ARCHITECTURAL TRANSLATION: RUST VS. MODERN C++20
    8.1 Memory Safety: Rust Borrow Checker vs. `std::span` Views
    8.2 Monadic Error Handling vs. C++ Exception Hierarchies
    8.3 Value Semantics & Compile-Time Safety
    8.4 C ABI Direct Integration vs. Rust FFI Bindings
    8.5 Concurrency Models: Single-Threaded CLI vs. Multi-Threaded Qt

9.  QT 6 GRAPHICAL USER INTERFACE ARCHITECTURE
    9.1 Non-Blocking Concurrency: The `DeviceWorker` / `QThread` Pattern
    9.2 Thread-Safe Inter-Thread Signal/Slot Marshalling & Meta-Types
    9.3 Tab-Widget Hierarchical Architecture (Samples View vs. Patterns View)
    9.4 Interactive Sequencer Pattern Manager & Hardware Renaming UI
    9.5 Multi-Sample Extended Selection Mode (`QAbstractItemView::ExtendedSelection`)
    9.6 Multi-Slot Batch Operations: Batch Download, Batch Erase, and Context Menu Helpers
    9.7 Multi-File Drag-and-Drop Ingestion Engine
    9.8 Interactive Waveform Visualizer (`WaveformWidget` & Peak Decimation)
    9.9 Interactive Sample Chopper Dialog (`SampleChopperDialog`) with Live Target Status
    9.10 Preset Package Download & Restore Wizards

10. FIELD DEBUGGING & REAL-WORLD POST-MORTEM
    10.1 The Infamous "No space left on device" (-ENOSPC) ALSA Bug
    10.2 The Silent Audio Playback Failure in Modern Linux Desktop Environments
    10.3 Embedded USB Microcontroller FIFO Overruns
    10.4 The `SampleFull` Sector Exhaustion Failure During Package Uploads
    10.5 The Qt `slots` Preprocessor Macro Keyword Collision Trap

11. COMMAND-LINE INTERFACE (`volsa2-cli`) REFERENCE MANUAL
    11.1 CLI Design Philosophy & Execution Mechanics
    11.2 Subcommand Reference: `list`, `download`, `upload`, `remove`
    11.3 Package Subcommands: `download-package` (`pkg-dl`), `upload-package` (`pkg-up`)
    11.4 Automation & Shell Scripting Integration

12. QUALITY ASSURANCE, TEST SUITES & VERIFICATION
    12.1 Property-Based 7-Bit Round-Trip Testing
    12.2 Golden-File Hardware Capture Validation (Samples 1–14)
    12.3 DSP Precision & Mathematical Resampling Tests
    12.4 Mock ALSA Loopback Verification
    12.5 Sequencer Pattern SysEx Verification (`test_pattern_proto`)
    12.6 Package Archival & Round-Trip Deserialization Tests (`test_package`)
    12.7 Transient Chopper Detection & Slicing Tests (`test_sample_chopper`)

13. BUILD SYSTEM, DEPLOYMENT & TROUBLESHOOTING
    13.1 Dependency Matrix & System Prerequisites
    13.2 CMake Build Configurations & Optimization Flags
    13.3 Linux udev Permissions & Real-Time Audio Configuration
    13.4 Diagnostic Toolkit (`amidi`, `aconnect`, `aplay`, `evtest`)

14. SUMMARY & CONCLUSION
===================================================================================
```

---

## 1. Executive Summary & Hardware Overview

### 1.1 Hardware Lineage: Volca Sample 1 vs. Volca Sample 2
The KORG Volca Sample series represents one of the most popular portable hardware sample sequencers in modern electronic music production. However, a massive architectural divergence exists between the original **Volca Sample (1st Generation)** and the **Volca Sample 2 (2nd Generation)**:

| Hardware Attribute | Original Volca Sample (Gen 1) | Volca Sample 2 (Gen 2) |
|---|---|---|
| **Data Transfer Interface** | 3.5mm Analog Audio Sync Jack | Micro-USB Class-Compliant USB MIDI |
| **Physical Modulation** | Audio Frequency-Shift Keying (FSK) | High-Speed USB 2.0 Packets / MIDI 1.0 SysEx |
| **Data Transfer Rate** | ~3.5 kB/sec (Audio rate modulation) | ~31.25 kB/sec to 100+ kB/sec (SysEx stream) |
| **Transfer Reliability** | Extremely sensitive to audio volume, EQ, distortion | Bit-perfect deterministic digital transmission |
| **Bi-directional Query** | None (Unidirectional receive only) | Full Duplex (Upload, download, memory queries) |
| **Sample Storage Capacity** | 100 sample slots (indexed 0–99), 4 MB Flash | **200 sample slots (indexed 0–199), 8 MB Flash** |
| **Native Sample Format** | 31.25 kHz, 16-bit linear mono PCM | **31.25 kHz, 16-bit linear mono PCM** |
| **Host Connectivity** | Requires dedicated audio interface output | Standard USB connection to PC/Mac/Linux |

While the original Volca Sample required users to play audio chirp signals through headphone jacks, the Volca Sample 2 is an entirely digital USB MIDI peripheral. It exposes a bi-directional MIDI System Exclusive communication protocol, enabling complete two-way sample librarianship.

### 1.2 Electrical, USB & Microcontroller Architecture
Internally, the Volca Sample 2 incorporates:
- A dedicated **ARM Cortex-M** class 32-bit microcontroller handling the USB physical layer, MIDI decoding, sequencer logic, and flash memory bus.
- An **8 MB NAND/NOR Flash Memory** chip dedicated to storing audio PCM sample data and slot metadata.
- A high-quality digital-to-analog converter (DAC) coupled with analog isolators and reconstruction filters, clocked to an exact sample playback rate of **31,250 Hz**.
- A USB Full-Speed (12 Mbps) interface operating under standard **USB Audio/MIDI Class 1.0** specifications.

The microcontroller does **not** run an embedded operating system (such as Linux or RTOS); it runs a bare-metal firmware loop. Because its USB input FIFO buffer is hardware-constrained (typically 64 to 256 bytes deep), sending large bursts of data at full USB bandwidth results in immediate FIFO overruns, dropping packets silently and causing the hardware state machine to lock up.

### 1.3 Internal Flash Memory Organization & Sector Allocation
The 8 MB internal storage is partitioned into fixed-size **sectors**.
- Rather than maintaining an arbitrary byte-addressed filesystem (like FAT32 or ext4), the Volca Sample 2 firmware organizes storage into uniform sector blocks.
- When an audio sample is uploaded, it is allocated contiguous sectors sufficient to hold its PCM payload.
- The total capacity of the machine is reported via the `SampleSpaceDump` command as:
  - $\text{Total Sectors}$: The maximum number of sector units across the entire 8 MB flash.
  - $\text{Used Sectors}$: The number of sectors currently occupied by active sample slots.
- Memory fragmentation can occur if samples of various lengths are repeatedly uploaded and deleted. In extreme cases, a sample might fail to upload even if the reported free sectors appear sufficient, if contiguous sectors cannot be allocated.

### 1.4 Native Audio Hardware Architecture (31,250 Hz 16-bit Mono PCM)
The playback engine of the Volca Sample 2 is strictly tuned to a non-standard sampling rate:
$$\mathbf{F_s = 31,250\text{ Hz} \quad (31.25\text{ kHz})}$$

#### Why 31,250 Hz?
The choice of 31,250 Hz is an intentional design choice deeply rooted in digital synthesis and MIDI heritage:
1. Standard MIDI baud rate is exactly $31,250\text{ baud}$ ($31.25\text{ kbps}$). Microcontroller clock prescalers can derive $31,250\text{ Hz}$ with zero fractional clock jitter from standard 8 MHz, 12 MHz, or 48 MHz crystals.
2. The Nyquist limit for 31,250 Hz is:
   $$f_{\text{Nyquist}} = \frac{31250}{2} = 15,625\text{ Hz} \quad (15.625\text{ kHz})$$
   This provides full audio bandwidth up to 15 kHz—covering the entire frequency spectrum of drum machines, percussion, basses, and vintage digital samplers—while reducing the flash memory consumption per sample by **29.1%** compared to 44.1 kHz, and by **34.9%** compared to 48.0 kHz.
3. Every sample is stored as a sequence of **16-bit signed two's complement integers** (`int16_t`, Little-Endian byte order), spanning values from $-32768$ to $+32767$.
4. The audio engine is strictly **single-channel (monaural)**. Stereo panning on the Volca is performed in real-time by the mixer engine during sequencer playback, not stored inside the sample memory itself.

### 1.5 Architectural Goals & Constraints of VolSa 2
The official KORG librarian software for the Volca Sample 2 is an Electron-based desktop application that is officially supported only on Windows and macOS. It suffers from heavy memory overhead, lack of command-line automation, and complete absence of Linux support.

The mission of **VolSa 2** is to provide an industrial-strength, native C++20 and Qt 6 implementation engineered specifically for the Linux ecosystem:
- **Zero Daemon Dependencies:** Interfaces directly with the Linux kernel via the ALSA library (`libasound`), requiring no JACK, PulseAudio, or PipeWire background daemons for MIDI communication.
- **Deterministic Audio DSP:** Integrates gold-standard band-limited Sinc polyphase resampling (`libsamplerate`) and multi-format audio decoding (`libsndfile`) to transform any audio file into pristine 31.25 kHz PCM.
- **Direct ALSA PCM Audio Auditioning:** Bypasses broken desktop multimedia frameworks to stream sample previews directly through ALSA PCM hardware devices.
- **Fail-Safe Flow Control:** Implements a deterministic packet chunking and throttling protocol that prevents embedded USB FIFO overflows.
- **Complete Feature Parity:** Provides both an interactive command-line binary (`volsa2-cli`) and a rich desktop application (`volsa2-gui`) with interactive waveform navigation, drag-and-drop, and batch library backup.

---

## 2. The Complete "From Scratch" Engineering Blueprint

If you were to sit down at a blank text editor and build VolSa 2 from absolute zero, how does the system come together? What are the layers, and how do they communicate?

This section provides the master blueprint for the entire software engineering process.

```
+----------------------------------------------------------------------------------------------------+
|                                    VOLSA 2 ARCHITECTURAL TOPOLOGY                                  |
+----------------------------------------------------------------------------------------------------+
|                                                                                                    |
|   +--------------------------------------------------------------------------------------------+   |
|   | LAYER 6: PRESENTATION LAYER (Qt 6 GUI & POSIX CLI)                                         |   |
|   | - MainWindow: 200-slot QTableWidget, live search filter, selection debouncer               |   |
|   | - WaveformWidget: QPainter peak-decimated audio canvas, animated playhead, seek scrubber   |   |
|   | - UploadDialog: Audio metadata preview, stereo-to-mono mode selector, audio auditioning    |   |
|   | - CLI: Headless POSIX argument parser, ANSI colored terminal reporting, batch scripts      |   |
|   +--------------------------------------------------------------------------------------------+   |
|                                                |                                                   |
|                        Queued Signals / Slots  |  Synchronous Function Calls                       |
|                                                v                                                   |
|   +--------------------------------------------------------------------------------------------+   |
|   | LAYER 5: THREAD WORKER & ORCHESTRATION LAYER                                               |   |
|   | - DeviceWorker: Runs on secondary QThread, isolates UI from blocking ALSA calls            |   |
|   | - AlsaAudioPlayer: Background PCM playback thread, zero-dependency ALSA sound engine       |   |
|   +--------------------------------------------------------------------------------------------+   |
|                                                |                                                   |
|                                                v                                                   |
|   +--------------------------------------------------------------------------------------------+   |
|   | LAYER 4: HARDWARE DRIVER & FLOW CONTROL LAYER (device.hpp / device.cpp)                    |   |
|   | - Device: High-level device API (connect, query_space, get_slot, upload_slot, erase_slot)  |   |
|   | - 256-byte SysEx chunking engine with precise inter-packet cooldown timer                  |   |
|   | - Non-blocking ALSA poll loop & multi-event SysEx stream reassembler                       |   |
|   | - Active Sensing (0xFE) and MIDI Clock (0xF8) packet filter & drainer                      |   |
|   +--------------------------------------------------------------------------------------------+   |
|                        |                                               |                           |
|                        v                                               v                           |
|   +------------------------------------------+   +---------------------------------------------+   |
|   | LAYER 2: PROTOCOL ENGINE (proto.hpp)     |   | LAYER 3: AUDIO DSP ENGINE (audio.hpp)       |   |
|   | - SysEx framing (EST 0xF0, EOX 0xF7)     |   | - Multi-codec decoding via libsndfile       |   |
|   | - KORG header serialization              |   | - Stereo downmixing (Mid, Left, Right, Side)|   |
|   | - Message types: Search, Space, Header,  |   | - Band-limited sinc resampling via          |   |
|   |   Data Dump, Status ACK/NAK              |   |   libsamplerate (SRC_SINC_BEST_QUALITY)     |   |
|   | - Binary sample header struct (32 bytes) |   | - Dynamic range clamping & int16 conversion |   |
|   +------------------------------------------+   | - 44-byte RIFF/WAVE serializer & writer     |   |
|                        |                         +---------------------------------------------+   |
|                        v                                                                           |
|   +------------------------------------------+                                                     |
|   | LAYER 1: BITWISE ENCODING (seven_bit.hpp)|                                                     |
|   | - U7 value wrapper (0x00 - 0x7F)         |                                                     |
|   | - 7-to-8 octet bit packer (U8ToU7)       |                                                     |
|   | - 8-to-7 octet bit unpacker (U7ToU8)     |                                                     |
|   | - Bitwise MSB header injection/extraction|                                                     |
|   +------------------------------------------+                                                     |
|                        |                                                                           |
|                        v                                                                           |
|   +--------------------------------------------------------------------------------------------+   |
|   | LAYER 0: LINUX KERNEL ALSA SUBSYSTEM (libasound.so)                                        |   |
|   | - snd_seq: Sequencer client/port management, subscriptions, FIFO event delivery            |   |
|   | - snd_pcm: PCM ring buffers, hardware parameters, interleaved audio frame streaming        |   |
|   +--------------------------------------------------------------------------------------------+   |
|                                                |                                                   |
|                                                v                                                   |
|   +--------------------------------------------------------------------------------------------+   |
|   | PHYSICAL TARGET: KORG VOLCA SAMPLE 2 HARDWARE                                              |   |
|   | - Class-compliant USB MIDI endpoint                                                        |   |
|   | - Embedded ARM Cortex-M USB FIFO buffer & 8 MB Flash Memory storage                        |   |
|   +--------------------------------------------------------------------------------------------+   |
+----------------------------------------------------------------------------------------------------+
```

### 2.2 Seven-Stage Step-by-Step Implementation Roadmap

Building VolSa 2 follows a disciplined 7-stage engineering trajectory:

```
[Stage 1: 7-Bit Bitwise Foundations]
                │
                ▼
[Stage 2: KORG SysEx Protocol Engine]
                │
                ▼
[Stage 3: Audio DSP & Sample Conversion]
                │
                ▼
[Stage 4: ALSA Sequencer Hardware Driver]
                │
                ▼
[Stage 5: Command-Line Interface (CLI)]
                │
                ▼
[Stage 6: Embedded ALSA PCM Audio Engine]
                │
                ▼
[Stage 7: Multi-Threaded Qt 6 GUI]
```

#### Stage 1: The 7-Bit Bitwise Foundations (`seven_bit.hpp`, `seven_bit.cpp`)
- **Objective:** Solve the fundamental MIDI constraint: data bytes cannot have the most significant bit (MSB, bit 7) set to 1.
- **Tasks:**
  1. Define a strongly typed `U7` struct wrapping `uint8_t` ensuring invariants $0 \le x \le 127$.
  2. Implement `U8ToU7`: An octet packing algorithm grouping 7 raw 8-bit bytes into 8 7-bit bytes, storing the MSBs in a leading container byte.
  3. Implement `U7ToU8`: The inverse decoding algorithm recovering the original 8-bit payload.
  4. Write exhaustive property-based unit tests verifying that $U7ToU8(U8ToU7(B)) \equiv B$ for all byte slices.

#### Stage 2: KORG SysEx Protocol Engine (`proto.hpp`, `proto.cpp`)
- **Objective:** Implement binary serializers and deserializers for every KORG Volca Sample 2 SysEx message.
- **Tasks:**
  1. Define constants: `EST = 0xF0`, `EOX = 0xF7`, `KORG_ID = 0x42`, `VOLCA_FAMILY = 0x2D`.
  2. Implement header builders: 2-byte broadcast header and 6-byte extended header with channel bitwise math (`0x30 | channel`).
  3. Implement data structures for:
     - `SearchDeviceRequest` & `SearchDeviceReply`
     - `SampleSpaceDumpRequest` & `SampleSpaceDump`
     - `SampleHeaderDumpRequest` & `SampleHeader` (including the 32-byte unpacked struct)
     - `SampleDataDumpRequest` & `SampleData`
     - Status response codes (`ACK_STATUS = 0x23`, `NakStatus::Busy = 0x24`, `SampleFull = 0x25`, `DataFormat = 0x26`).
  4. Validate against real hardware byte dumps captured from a physical Volca.

#### Stage 3: High-Fidelity Audio DSP Engine (`audio.hpp`, `audio.cpp`)
- **Objective:** Convert arbitrary audio files (MP3, FLAC, high-res WAV) into the Volca's native 31,250 Hz 16-bit mono format.
- **Tasks:**
  1. Use `libsndfile` to decode audio containers into double-precision floating-point arrays $[-1.0, +1.0]$.
  2. Implement stereo downmixing matrices: Mid, Left, Right, Side.
  3. Integrate `libsamplerate` using `SRC_SINC_BEST_QUALITY` to perform band-limited sinc interpolation with optimal anti-aliasing low-pass filtering.
  4. Quantize floating-point samples to 16-bit signed integers (`int16_t`) with hard clamping $[-32768, 32767]$.
  5. Implement a 44-byte standard RIFF WAVE serializer to save downloaded samples to disk.

#### Stage 4: Low-Level ALSA Sequencer Driver (`device.hpp`, `device.cpp`)
- **Objective:** Establish reliable, bi-directional communication with the Volca hardware over USB MIDI.
- **Tasks:**
  1. Open ALSA Sequencer client (`snd_seq_open`) and register a duplex application port.
  2. Query kernel clients to automatically discover the client and port named `"volca sample"`.
  3. Establish bi-directional subscriptions between the local port and the hardware port.
  4. Implement flow-controlled chunked SysEx transmission: slice payloads into 256-byte blocks with a 10 ms cooldown.
  5. Implement non-blocking event polling and multi-chunk SysEx reassembly.
  6. Filter out high-frequency MIDI Clock (`0xF8`) and Active Sensing (`0xFE`) events.

#### Stage 5: Headless Command-Line Interface (`cli/main.cpp`)
- **Objective:** Provide a fast, scriptable terminal tool for studio automation.
- **Tasks:**
  1. Implement POSIX argument parsing for commands: `list`, `download`, `upload`, `remove`.
  2. Support slot auto-detection, overwrite warnings, interactive backup confirmation, and dry-run modes.
  3. Implement real-time ANSI terminal progress indicators during sample transfers.

#### Stage 6: Embedded ALSA PCM Audio Audition Engine (`audio_player.hpp`, `audio_player.cpp`)
- **Objective:** Deliver rock-solid sample preview playback without relying on broken desktop multimedia plugins.
- **Tasks:**
  1. Open the ALSA PCM default playback stream (`snd_pcm_open`).
  2. Configure hardware parameters (`snd_pcm_set_params`) for 31,250 Hz 16-bit PCM.
  3. Implement automatic mono-to-stereo sample replication fallback for DACs that do not natively accept single-channel streams.
  4. Run playback on an isolated worker thread with atomic control flags for Play, Stop, and Position reporting.

#### Stage 7: Multi-Threaded Qt 6 Graphical User Interface (`gui/`)
- **Objective:** Build a responsive, visual desktop librarian application.
- **Tasks:**
  1. Implement `DeviceWorker` running on an independent `QThread` to isolate blocking ALSA operations from the GUI event loop.
  2. Create `MainWindow` featuring a 200-slot table view with instant search filtering.
  3. Create `WaveformWidget`: a custom `QWidget` rendering peak-decimated audio waveforms with an interactive playhead.
  4. Build `UploadDialog`: a wizard for file inspection, stereo mode selection, waveform preview, and auditioning.
  5. Implement drag-and-drop ingestion and batch export operations.

---

### 2.3 End-to-End Upload Dataflow: WAV on Disk to Volca Flash

To understand how the entire system functions as a cohesive whole, follow the exact life cycle of an audio file being uploaded to slot 42:

```
+----------------------------------------------------------------------------------------------------+
|                                    SAMPLE UPLOAD LIFECYCLE TRACE                                   |
+----------------------------------------------------------------------------------------------------+
  1. USER ACTION: User drags "snare.wav" (44.1 kHz, 24-bit stereo) into VolSa 2 and selects Slot 42.
  2. AUDIO DECODING:
     - libsndfile parses RIFF chunks and decodes interleaved audio into double-precision float array.
  3. STEREO DOWNMIXING:
     - User selects "Mid Mode": y[n] = (Left[n] + Right[n]) * 0.5. Array is converted to mono.
  4. SINC RESAMPLING:
     - libsamplerate applies SRC_SINC_BEST_QUALITY with ratio ρ = 31250.0 / 44100.0 = 0.70861678.
     - Anti-aliasing sinc filter eliminates frequencies above 15.625 kHz.
  5. QUANTIZATION:
     - Float values in [-1.0, 1.0] are scaled by 32767.0, rounded, and clamped into int16_t buffer.
  6. METADATA HEADER GENERATION:
     - 32-byte binary struct constructed: Name="snare", Length=N samples, Level=65535, Speed=16384.
     - 32-byte struct is 7-bit packed via U8ToU7 into 37 bytes.
     - Extended KORG header prepended: [F0 42 30 00 01 2D 4E 2A 00 <37 bytes> F7].
  7. HEADER TRANSMISSION & ACK:
     - ALSA Sequencer transmits 47-byte SysEx message to Volca.
     - Volca hardware reserves flash memory and replies with ACK: [F0 42 30 00 01 2D 23 F7].
     - VolSa 2 verifies ACK byte (0x23).
  8. RAW PCM PAYLOAD PACKING:
     - 16-bit PCM buffer (2N bytes) is serialized to Little-Endian bytes.
     - Byte array is packed via U8ToU7 into M bytes of 7-bit MIDI data.
     - Extended KORG header prepended: [F0 42 30 00 01 2D 4F 2A 00 <M bytes> F7].
  9. FLOW-CONTROLLED CHUNK DISPATCH:
     - Message is sliced into 256-byte chunks.
     - For each chunk:
       - Dispatched via snd_seq_event_output_direct().
       - snd_seq_drain_output() flushes the kernel buffer.
       - Worker thread sleeps for 10 ms (inter-chunk cooldown).
       - Drain pending non-SysEx events from input buffer.
 10. FINAL ACK & REFRESH:
     - Volca finishes writing to physical flash memory.
     - Volca replies with final ACK: [F0 42 30 00 01 2D 23 F7].
     - DeviceWorker emits slotLoaded(42) and spaceUpdated() signals.
     - MainWindow updates table row 42 and refreshes the memory usage progress bar.
+----------------------------------------------------------------------------------------------------+
```

---

### 2.4 End-to-End Download Dataflow: Volca Flash to RIFF WAV File

The reverse operation—downloading audio from slot 15 and saving it as `sample15.wav`—proceeds through the following deterministic pipeline:

```
+----------------------------------------------------------------------------------------------------+
|                                   SAMPLE DOWNLOAD LIFECYCLE TRACE                                  |
+----------------------------------------------------------------------------------------------------+
  1. REQUEST METADATA:
     - VolSa 2 transmits SampleHeaderDumpRequest for slot 15:
       [F0 42 30 00 01 2D 1E 0F 00 F7].
  2. RECEIVE METADATA:
     - Volca replies with SampleHeader (47 bytes total).
     - Device::receive_raw() reassembles incoming SysEx bytes until EOX (0xF7).
     - Parser validates extended header and extracts 37-byte packed payload.
     - U7ToU8 unpacks 37 bytes into the original 32-byte binary header.
     - Little-Endian integers are decoded: Name="BASS_SUB", Length=28400 samples.
  3. REQUEST PCM DATA:
     - VolSa 2 transmits SampleDataDumpRequest for slot 15:
       [F0 42 30 00 01 2D 1F 0F 00 F7].
  4. RECEIVE PCM DUMP:
     - Volca hardware reads physical flash sectors and streams SampleData SysEx packet.
     - Payload size for 28,400 samples = 56,800 bytes of 8-bit PCM = 64,915 bytes of 7-bit data.
     - ALSA Sequencer receives multi-kilobyte payload across sequential kernel event buffers.
     - VolSa 2 non-blocking loop accumulates chunks until trailing 0xF7 is received.
  5. DECODE AUDIO PAYLOAD:
     - Parser strips extended header (9 bytes) and trailing EOX.
     - U7ToU8 unpacks 64,915 7-bit bytes back into 56,800 raw 8-bit bytes.
     - Byte pairs are assembled into 28,400 signed 16-bit integers (int16_t) via Little-Endian math:
       sample[i] = static_cast<int16_t>(raw[2*i] | (raw[2*i+1] << 8)).
  6. RIFF/WAVE CONTAINER SERIALIZATION:
     - 44-byte standard RIFF WAVE header is formatted:
       - SampleRate = 31250 Hz, Channels = 1, BitsPerSample = 16, ByteRate = 62500 B/s.
     - Header and PCM byte buffer are written to disk as "sample15.wav".
+----------------------------------------------------------------------------------------------------+
```

---

## 3. The Linux Audio Infrastructure: Comprehensive ALSA Interface

VolSa 2 interacts with the Linux kernel audio architecture entirely through **ALSA** (Advanced Linux Sound Architecture), implemented in user-space via `libasound.so` and `#include <alsa/asoundlib.h>`.

This section introduces every subsystem, data structure, and system call needed to master ALSA for both MIDI control and PCM digital audio.

```
+-----------------------------------------------------------------------------------+
|                               LINUX ALSA ARCHITECTURE                             |
+-----------------------------------------------------------------------------------+
|  USER SPACE                                                                       |
|  +-----------------------------------------------------------------------------+  |
|  |                        VolSa 2 Application                                  |  |
|  |       [volsa2::Device]                              [AlsaAudioPlayer]       |  |
|  +--------------|----------------------------------------------|---------------+  |
|                 | (MIDI Sequencer API)                         | (PCM API)        |
|                 v                                              v                  |
|  +-----------------------------------------------------------------------------+  |
|  |                        ALSA Library (libasound.so)                          |  |
|  |       snd_seq_* functions                           snd_pcm_* functions     |  |
|  +--------------|----------------------------------------------|---------------+  |
|                 |                                              |                  |
|=================|==============================================|==================|
|  KERNEL SPACE   v (/dev/snd/seq)                               v (/dev/snd/pcmC*D*|
|  +-------------------------------------+      +--------------------------------+  |
|  | snd-seq (ALSA Sequencer Subsystem)  |      | snd-pcm (ALSA PCM Subsystem)   |  |
|  | - Client & Port Management          |      | - DMA Ring Buffers             |  |
|  | - Subscription Router & FIFO Queues |      | - Hardware Parameters & Clocks |  |
|  +------------------|------------------+      +----------------|---------------+  |
|                     v                                          v                  |
|  +-----------------------------------------------------------------------------+  |
|  | snd-usb-audio (USB Audio & MIDI Class Driver)                               |  |
|  +-----------------------------------------------------------------------------+  |
|                                         | USB Bus                                 |
|=========================================|=========================================|
|  HARDWARE                               v                                         |
|  +-----------------------------------------------------------------------------+  |
|  | KORG Volca Sample 2 Hardware (USB MIDI) & Computer Audio DAC (Headphones)   |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

### 3.1 Architecture of the Linux Sound Subsystem
The Linux kernel audio stack is structured into distinct tiers:
1. **Kernel Core (`sound/core`):** Houses the fundamental audio drivers (`snd`), memory management, timer infrastructure, and device node dispatchers.
2. **Device Drivers:**
   - `snd-usb-audio`: Standard driver for all USB Audio and USB MIDI class-compliant hardware.
   - `snd-hda-intel`, `snd-soc-*`: Drivers for on-board audio codecs and DACs.
3. **User-Space ALSA Library (`libasound`):** Provides a stable, standardized C API insulating applications from kernel ioctl intricacies.
4. **Sound Servers (PipeWire, PulseAudio, JACK):** High-level audio mixing daemons. While modern Linux desktops route user audio through PipeWire, the underlying hardware interfaces remain ALSA device nodes. VolSa 2 communicates directly with ALSA, ensuring zero dependence on external daemons.

---

### 3.2 The ALSA MIDI Sequencer Subsystem (`snd_seq_*`)

#### 3.2.1 Why Sequencer API vs. RawMIDI (`/dev/snd/midiC*D*`)
Linux exposes two distinct MIDI interfaces:
- **RawMIDI (`snd_rawmidi_*` / `/dev/snd/midiC*D*`):** A primitive character-device stream. It offers direct access to a hardware endpoint, but is strictly exclusive: only one process can open the device at a time. Furthermore, it lacks automatic routing, filtering, or virtual port abstractions.
- **ALSA Sequencer (`snd_seq_*` / `/dev/snd/seq`):** A high-level, event-driven messaging bus.
  - Supports arbitrary client-to-client subscriptions.
  - Allows multiple applications to share MIDI devices simultaneously.
  - Provides hardware discovery and dynamic enumeration.
  - Delivers structured events (`snd_seq_event_t`) with hardware timestamps.

VolSa 2 utilizes the **ALSA Sequencer API** exclusively.

#### 3.2.2 Client Lifecycle, Port Creation, and Duplex Capabilities
To communicate via the Sequencer, an application registers as a sequencer client and creates one or more ports.

##### 1. Opening the Client Handle:
```cpp
snd_seq_t* seq = nullptr;
int err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
if (err < 0) {
    throw std::runtime_error("Failed to open ALSA sequencer: " + std::string(snd_strerror(err)));
}
```
The mode `SND_SEQ_OPEN_DUPLEX` establishes both input and output capabilities.

##### 2. Identifying the Client:
```cpp
snd_seq_set_client_name(seq, "VolSa2");
```
Setting a human-readable name allows system utilities (such as `aconnect -l` or `qjackctl`) to display the client.

##### 3. Registering a Duplex Port:
Ports act as connection jacks. VolSa 2 creates a single duplex port:
```cpp
int port_id = snd_seq_create_simple_port(
    seq,
    "VolSa2 Port",
    SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE |
    SND_SEQ_PORT_CAP_READ  | SND_SEQ_PORT_CAP_SUBS_READ  |
    SND_SEQ_PORT_CAP_DUPLEX,
    SND_SEQ_PORT_TYPE_MIDI_GENERIC | 
    SND_SEQ_PORT_TYPE_APPLICATION  | 
    SND_SEQ_PORT_TYPE_PORT
);
```

**Port Capability Flags Explained:**
- `SND_SEQ_PORT_CAP_WRITE`: Allows other clients to send events into this port.
- `SND_SEQ_PORT_CAP_SUBS_WRITE`: Allows other clients to subscribe to send events to this port.
- `SND_SEQ_PORT_CAP_READ`: Allows other clients to receive events from this port.
- `SND_SEQ_PORT_CAP_SUBS_READ`: Allows other clients to subscribe to receive events from this port.
- `SND_SEQ_PORT_CAP_DUPLEX`: Declares that this port handles bidirectional traffic.

#### 3.2.3 Dynamic Client Enumeration & Automated Hardware Discovery
Rather than requiring the user to specify complex ALSA hardware addresses (e.g. `hw:2,0,0`), VolSa 2 dynamically queries the kernel sequencer client graph:

```cpp
snd_seq_client_info_t* cinfo;
snd_seq_client_info_alloca(&cinfo);
snd_seq_client_info_set_client(cinfo, -1); // Start search from beginning

int volca_client = -1;
int volca_port   = -1;

while (snd_seq_query_next_client(seq, cinfo) >= 0) {
    int client_id = snd_seq_client_info_get_client(cinfo);
    const char* client_name = snd_seq_client_info_get_name(cinfo);
    
    if (client_name && std::string_view(client_name) == "volca sample") {
        // Enumerate ports belonging to this client
        snd_seq_port_info_t* pinfo;
        snd_seq_port_info_alloca(&pinfo);
        snd_seq_port_info_set_client(pinfo, client_id);
        snd_seq_port_info_set_port(pinfo, -1);
        
        if (snd_seq_query_next_port(seq, pinfo) >= 0) {
            volca_client = client_id;
            volca_port   = snd_seq_port_info_get_port(pinfo);
            break; // Hardware discovered!
        }
    }
}
```

#### 3.2.4 Subscription Management & Routing Topology
Once the hardware endpoint is identified, two subscriptions are established using `snd_seq_port_subscribe_t`:
1. **VolSa 2 $\to$ Volca Sample 2:** Outgoing command path.
2. **Volca Sample 2 $\to$ VolSa 2:** Incoming response path.

```cpp
snd_seq_port_subscribe_t* sub;
snd_seq_port_subscribe_alloca(&sub);

snd_seq_addr_t sender{ static_cast<unsigned char>(my_client), static_cast<unsigned char>(my_port) };
snd_seq_addr_t dest{ static_cast<unsigned char>(volca_client), static_cast<unsigned char>(volca_port) };

// Connect Outgoing: My Port -> Volca Port
snd_seq_port_subscribe_set_sender(sub, &sender);
snd_seq_port_subscribe_set_dest(sub, &dest);
snd_seq_subscribe_port(seq, sub);

// Connect Incoming: Volca Port -> My Port
snd_seq_port_subscribe_set_sender(sub, &dest);
snd_seq_port_subscribe_set_dest(sub, &sender);
snd_seq_subscribe_port(seq, sub);
```

#### 3.2.5 Event Architecture (`snd_seq_event_t`) & Direct Dispatch
MIDI data within ALSA is packaged in `snd_seq_event_t` structures. For SysEx payloads, the event type is set to `SND_SEQ_EVENT_SYSEX`, and the pointer to the raw byte buffer is attached to the variable payload union:

```cpp
snd_seq_event_t ev;
snd_seq_ev_clear(&ev);
snd_seq_ev_set_direct(&ev); // Bypass queue timing; deliver immediately
snd_seq_ev_set_source(&ev, my_port);
snd_seq_ev_set_subs(&ev);   // Broadcast to all subscribed listeners
ev.type = SND_SEQ_EVENT_SYSEX;
ev.flags |= SND_SEQ_EVENT_LENGTH_VARIABLE;
ev.data.ext.len = chunk_len;
ev.data.ext.ptr = const_cast<uint8_t*>(chunk_data);

// Dispatch directly to kernel output buffer
snd_seq_event_output_direct(seq, &ev);
snd_seq_drain_output(seq);
```

#### 3.2.6 Kernel Event Pool & Buffer Sizing Mechanics
By default, the ALSA Sequencer kernel driver allocates a modest client event pool (often only 200 input cells). When downloading a 100 kB sample dump, the kernel event pool is overwhelmed within milliseconds, dropping events and throwing `-ENOSPC` errors.

To accommodate large SysEx data dumps, VolSa 2 expands both the kernel input pool and the user-space output buffer immediately after opening the sequencer:
```cpp
// Expand kernel input cell pool to 32,768 cells
snd_seq_set_client_pool_input(seq, 32768);

// Expand output buffer to 128 KB
snd_seq_set_output_buffer_size(seq, 131072);
```

#### 3.2.7 Non-Blocking Asynchronous Event Polling
If an ALSA sequencer client is left in blocking mode, calling `snd_seq_event_input()` blocks indefinitely if the device drops a packet or fails to reply, causing the calling thread to hang permanently.

VolSa 2 switches the sequencer handle into **non-blocking mode**:
```cpp
snd_seq_nonblock(seq, 1); // 1 = non-blocking, 0 = blocking
```
In non-blocking mode:
- If no events are waiting in the queue, `snd_seq_event_input()` returns immediately with `-EAGAIN`.
- The caller can sleep for a short duration (e.g. 1 ms) and poll until a timeout expires.

#### 3.2.8 Draining, Purging, and Active Sensing Flood Rejection
Real-world hardware instruments continuously emit MIDI real-time status bytes:
- **Active Sensing (`0xFE`):** Transmitted every 300 ms by MIDI controllers to detect disconnected cables.
- **Timing Clock (`0xF8`):** Transmitted 24 times per quarter note when the internal tempo clock is active.

If unhandled, these real-time events flood the input FIFO buffer, triggering false SysEx parser errors and buffer overruns. VolSa 2 addresses this with two defensive mechanisms:

```cpp
// 1. Purge all pending input events prior to issuing a new command
void Device::clear_input() {
    snd_seq_drop_input(seq_);
    snd_seq_event_t* ev = nullptr;
    while (snd_seq_event_input(seq_, &ev) >= 0) {
        // Discard all queued events
    }
}

// 2. Filter out non-SysEx events during reception
if (ev->type != SND_SEQ_EVENT_SYSEX) {
    continue; // Silently ignore Active Sensing, Clocks, CCs
}
```

---

### 3.3 The ALSA PCM Audio Subsystem (`snd_pcm_*`)

VolSa 2 includes an integrated audio auditioning engine (`AlsaAudioPlayer`) built directly on the **ALSA PCM API**. This allows users to preview 31.25 kHz audio samples before uploading, without relying on external multimedia frameworks.

#### 3.3.1 PCM Fundamentals: Frames, Periods, and Ring Buffers
Digital audio in ALSA is organized around specific spatial and temporal structures:
- **Sample:** A single numerical value representing acoustic amplitude at a point in time (e.g. one 16-bit signed integer).
- **Frame:** A collection of simultaneous samples across all active channels. For mono, $1\text{ frame} = 1\text{ sample} = 2\text{ bytes}$. For stereo, $1\text{ frame} = 2\text{ samples} = 4\text{ bytes}$.
- **Period:** The discrete chunk of frames after which the audio hardware issues an interrupt to request new data.
- **Ring Buffer:** A circular memory buffer containing multiple periods. The application writes frames to the head of the buffer; the DAC DMA controller reads frames from the tail.

```
+-------------------------------------------------------------------------------+
|                             ALSA PCM RING BUFFER                              |
+-------------------------------------------------------------------------------+
|  Period 0      |  Period 1      |  Period 2      |  Period 3      |  Period 4 |
|  [Frames 0..N] |  [Frames 0..N] |  [Frames 0..N] |  [Frames 0..N] |  [0..N]   |
+-------------------------------------------------------------------------------+
        ^                                               ^
        | DAC DMA Read Pointer                          | Application Write Pointer
```

#### 3.3.2 Hardware Configuration & Parameter Negotiation
Configuring an ALSA PCM device can be achieved using the high-level `snd_pcm_set_params()` helper:

```cpp
snd_pcm_t* pcm = nullptr;
int err = snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
if (err < 0) {
    throw std::runtime_error("Cannot open PCM device: " + std::string(snd_strerror(err)));
}

err = snd_pcm_set_params(
    pcm,
    SND_PCM_FORMAT_S16_LE,             // 16-bit signed Little-Endian
    SND_PCM_ACCESS_RW_INTERLEAVED,     // Interleaved sample access
    1,                                 // 1 channel (mono)
    31250,                             // Native Volca sample rate
    1,                                 // Allow software resampling if needed
    50000                              // Latency in microseconds (50 ms)
);
```

#### 3.3.3 The 31,250 Hz Mono Playback Challenge & Stereo Duplication Fallback
Modern high-definition audio DACs (such as Intel HDA, Realtek ALC codecs, and USB audio interfaces) natively support only standard consumer sample rates: 44,100 Hz, 48,000 Hz, 96,000 Hz, and 192,000 Hz. Furthermore, many hardware drivers do not support single-channel (mono) streams at the hardware level.

When opening `"default"` on modern Linux, the ALSA plugin layer (via PipeWire-ALSA or PulseAudio-ALSA) automatically performs rate conversion from 31,250 Hz to the hardware's native rate. However, if the hardware refuses single-channel mono allocation, `snd_pcm_set_params()` fails.

VolSa 2 overcomes this limitation with an automatic **Stereo Fallback Engine**:
1. It attempts to configure mono playback at 31,250 Hz.
2. If configuration fails, it reconfigures the PCM device for **2-channel stereo**.
3. During streaming, it duplicates each mono sample across both left and right channels:
   $$L[n] = y[n], \quad R[n] = y[n]$$
This guarantees playback compatibility across every Linux sound card on the market.

#### 3.3.4 Audio Buffer Underrun (XRUN) Recovery Patterns
If the application thread writing audio frames is preempted or delayed, the DAC DMA pointer catches up with the write pointer. The ring buffer runs out of valid audio data, resulting in an **underrun** (XRUN), and `snd_pcm_writei()` returns `-EPIPE`.

To recover without terminating playback:
```cpp
snd_pcm_sframes_t written = snd_pcm_writei(pcm, buffer_ptr, frames_to_write);
if (written < 0) {
    // Recover from underrun (-EPIPE) or suspend (-ESTRPIPE)
    written = snd_pcm_recover(pcm, static_cast<int>(written), 0);
    if (written < 0) {
        // Recovery failed; device in broken state
        break;
    }
}
```
Calling `snd_pcm_recover()` automatically restores the PCM state machine to `SND_PCM_STATE_PREPARED`, allowing transmission to resume seamlessly.

---

## 4. KORG MIDI System Exclusive (SysEx) Protocol Specification

Communication between the host computer and the Volca Sample 2 conforms to the MIDI 1.0 System Exclusive standard, wrapped in KORG-specific identification headers.

### 4.1 MIDI 1.0 System Exclusive Framing Standards
System Exclusive messages provide manufacturer-defined extensions to standard MIDI. Every SysEx message is framed by two status bytes:
- **`0xF0` (EST - Exclusive Start):** Signals the start of a manufacturer-specific transmission.
- **`0xF7` (EOX - End of Exclusive):** Signals the termination of the transmission.
- **Data Byte Invariant:** Every byte between `0xF0` and `0xF7` **must** have its most significant bit cleared ($b_7 = 0$, values $0 \dots 127$). Any byte with $b_7 = 1$ is treated by MIDI hardware as a real-time status byte, terminating the SysEx packet immediately.

### 4.2 KORG Manufacturer Framing: Standard vs. Extended Headers
KORG employs two header formats:

#### Format 1: Standard 2-Byte Broadcast Header
Used strictly for unaddressed broadcast queries (such as device discovery):
```
[0x00] 0xF0  (EST: Exclusive Start)
[0x01] 0x42  (KORG Manufacturer Identifier: 66)
```

#### Format 2: Extended 6-Byte Device Header
Used for all addressed transactions (upload, download, erase, space query):
```
[0x00] 0xF0  (EST: Exclusive Start)
[0x01] 0x42  (KORG Manufacturer Identifier: 66)
[0x02] 0x3g  (Format identifier 0x30 | Global MIDI Channel 'g')
[0x03] 0x00  (Model category code)
[0x04] 0x01  (Model sub-category code)
[0x05] 0x2D  (Volca Sample 2 Family Model Code: 45)
```

### 4.3 Global MIDI Channel Addressing & Math
Byte index 2 encodes the target device's global MIDI channel:
$$\text{Header Byte 2} = \text{0x30} \ | \ (\text{channel} \ \& \ \text{0x0F})$$

For example:
- Global Channel 1 (index 0) $\to \text{0x30}$
- Global Channel 2 (index 1) $\to \text{0x31}$
- Global Channel 10 (index 9) $\to \text{0x39}$
- Global Channel 16 (index 15) $\to \text{0x3F}$

---

### 4.4 Device Discovery & Handshake Protocol

Before issuing sample read/write commands, the host initiates a discovery handshake to determine the device's active MIDI channel and firmware version.

```
HOST (VolSa 2)                                      DEVICE (Volca Sample 2)
      |                                                        |
      | -------- SearchDeviceRequest (F0 42 50 00 2A F7) ----> |
      |                                                        |
      | <------- SearchDeviceReply (15 bytes) ---------------- |
      |          [F0 42 50 01 <ch> 2A 2D 01 08 00 ...]         |
```

#### 1. Outgoing: `SearchDeviceRequest` (6 bytes)
```
Byte 0: 0xF0 (EST)
Byte 1: 0x42 (KORG ID)
Byte 2: 0x50 (Inquiry Command: Search Device)
Byte 3: 0x00 (Sub-command: Request)
Byte 4: <echo> (Arbitrary 7-bit tracking value, default 0x2A / 42)
Byte 5: 0xF7 (EOX)
```

#### 2. Incoming: `SearchDeviceReply` (15 bytes)
```
Byte 0:  0xF0 (EST)
Byte 1:  0x42 (KORG ID)
Byte 2:  0x50 (Inquiry Command: Search Device)
Byte 3:  0x01 (Sub-command: Reply)
Byte 4:  <channel> (Active Global MIDI Channel: 0x00 - 0x0F)
Byte 5:  <echo>    (Reflected tracking value: 0x2A)
Byte 6:  0x2D \
Byte 7:  0x01  |-- VOLCA_SAMPLE_2_ID: [0x2D, 0x01, 0x08, 0x00]
Byte 8:  0x08  |
Byte 9:  0x00 /
Byte 10: <minor_lsb> (Firmware Minor Version LSB)
Byte 11: <minor_msb> (Firmware Minor Version MSB)
Byte 12: <major_lsb> (Firmware Major Version LSB)
Byte 13: <major_msb> (Firmware Major Version MSB)
Byte 14: 0xF7 (EOX)
```

**Firmware Version Decoding:**
```cpp
uint16_t minor = minor_lsb | (minor_msb << 7);
uint16_t major = major_lsb | (major_msb << 7);
// E.g., major=1, minor=2 -> Firmware Version 1.02
```

---

### 4.5 Sector Memory Query Protocol

Queries the total flash sector count and the number of currently occupied sectors.

#### 1. Outgoing: `SampleSpaceDumpRequest` (8 bytes)
```
Byte 0..5: Extended KORG Header [F0 42 3g 00 01 2D]
Byte 6:    0x1B (Command: Sample Space Dump Request)
Byte 7:    0xF7 (EOX)
```

#### 2. Incoming: `SampleSpaceDump` (12 bytes)
```
Byte 0..5:  Extended KORG Header [F0 42 3g 00 01 2D]
Byte 6:     0x4B (Command: Sample Space Dump Reply)
Byte 7:     <used_lsb> (Occupied sectors LSB)
Byte 8:     <used_msb> (Occupied sectors MSB)
Byte 9:     <all_lsb>  (Total available sectors LSB)
Byte 10:    <all_msb>  (Total available sectors MSB)
Byte 11:    0xF7 (EOX)
```

**Sector Calculation Formula:**
$$\text{Used Sectors} = \text{used\_lsb} \ | \ (\text{used\_msb} \ll 7)$$
$$\text{Total Sectors} = \text{all\_lsb} \ | \ (\text{all\_msb} \ll 7)$$
$$\text{Memory Occupancy Ratio} = \frac{\text{Used Sectors}}{\text{Total Sectors}}$$

---

### 4.6 Sample Metadata Header Specification

Sample metadata (name, length, playback speed, output volume) is stored as a 32-byte binary block, which is 7-bit packed into 37 SysEx payload bytes.

#### 1. Outgoing: `SampleHeaderDumpRequest` (10 bytes)
```
Byte 0..5: Extended KORG Header [F0 42 3g 00 01 2D]
Byte 6:    0x1E (Command: Sample Header Dump Request)
Byte 7:    <slot_lsb> (Slot index & 0x7F)
Byte 8:    <slot_msb> ((Slot index >> 7) & 0x7F)
Byte 9:    0xF7 (EOX)
```

#### 2. Incoming / Outgoing: `SampleHeader` (47 bytes total)
```
Byte 0..5:   Extended KORG Header [F0 42 3g 00 01 2D]
Byte 6:      0x4E (Command: Sample Header Dump Data)
Byte 7:      <slot_lsb> (Target slot index LSB)
Byte 8:      <slot_msb> (Target slot index MSB)
Byte 9..45:  [37 bytes: 7-bit packed payload of 32-byte header struct]
Byte 46:     0xF7 (EOX)
```

#### 4.6.1 32-Byte Unpacked Binary Memory Layout
When the 37 payload bytes are unpacked via `U7ToU8`, they yield exactly 32 bytes:

| Byte Offset | Field Name | Data Type | Endianness | Description |
|---|---|---|---|---|
| `0x00 .. 0x17` | `name` | `char[24]` | ASCII / UTF-8 | Sample name (null-padded, max 24 characters). |
| `0x18 .. 0x1B` | `length` | `uint32_t` | Little-Endian | Total sample count at 31,250 Hz. |
| `0x1C .. 0x1D` | `level` | `uint16_t` | Little-Endian | Output playback volume (Default: `0xFFFF` / 65535). |
| `0x1E .. 0x1F` | `speed` | `uint16_t` | Little-Endian | Playback pitch/speed (Default: `0x4000` / 16384). |

#### 4.6.2 Slot Erasure & Vacancy Conventions
The Volca Sample 2 identifies an **empty (unallocated) slot** through a specific signature:
- `name`: 24 zero bytes (`0x00`).
- `length`: `0` samples.
- `level`: `0`.
- `speed`: `0`.

To **erase a sample** from device memory:
1. Construct a zeroed 32-byte header struct.
2. 7-bit pack it into 37 bytes.
3. Transmit it as a `SampleHeader` message targeted at the desired slot index.
4. The Volca frees the associated flash sectors and replies with an `ACK_STATUS` (`0x23`).

---

### 4.7 Raw PCM Sample Data Protocol

Used to transfer the actual audio PCM waveform data.

#### 1. Outgoing: `SampleDataDumpRequest` (10 bytes)
```
Byte 0..5: Extended KORG Header [F0 42 3g 00 01 2D]
Byte 6:    0x1F (Command: Sample Data Dump Request)
Byte 7:    <slot_lsb> (Target slot index LSB)
Byte 8:    <slot_msb> (Target slot index MSB)
Byte 9:    0xF7 (EOX)
```

#### 2. Incoming / Outgoing: `SampleData` (Variable Length)
```
Byte 0..5:     Extended KORG Header [F0 42 3g 00 01 2D]
Byte 6:        0x4F (Command: Sample Data Dump Data)
Byte 7:        <slot_lsb> (Target slot index LSB)
Byte 8:        <slot_msb> (Target slot index MSB)
Byte 9..N-2:   [Variable length: 7-bit packed 16-bit LE PCM samples]
Byte N-1:      0xF7 (EOX)
```

For an audio sample containing $N$ samples:
- Raw binary size: $2N$ bytes (each sample is an `int16_t` Little-Endian pair).
- 7-bit packed size: $2N + \lceil 2N / 7 \rceil$ bytes.
- Total SysEx message size: $10 + \left( 2N + \lceil 2N / 7 \rceil \right)$ bytes.

---

### 4.8 Status Acknowledgment & NAK Protocol

After the host transmits a state-modifying command (such as a `SampleHeader` or `SampleData` upload), the Volca replies with an 8-byte status packet:

```
[0x00..0x05] Extended KORG Header [F0 42 3g 00 01 2D]
[0x06]       <status_byte>
[0x07]       0xF7 (EOX)
```

| Status Byte | Constant | Classification | Description |
|---|---|---|---|
| `0x23` | `ACK_STATUS` | **Positive ACK** | Operation succeeded. Flash written or slot erased. |
| `0x24` | `NakStatus::Busy` | **Negative ACK (NAK)** | Device processor is busy. Operation rejected; retry. |
| `0x25` | `NakStatus::SampleFull` | **Negative ACK (NAK)** | Insufficient free flash sectors to store sample. |
| `0x26` | `NakStatus::DataFormat` | **Negative ACK (NAK)** | Corrupt payload, invalid 7-bit packing, or length mismatch. |

---

### 4.9 Hardware Flow Control & USB Packet Slicing

Because the Volca Sample 2's USB receiver has a limited hardware FIFO, sending a continuous 60 kB SysEx dump at full USB speed causes an immediate buffer overflow.

#### The VolSa 2 Flow Control Strategy:
1. **Packet Slicing:** Large SysEx messages are partitioned into chunks of at most **256 bytes**.
2. **Direct Dispatch:** Each chunk is transmitted as a discrete `SND_SEQ_EVENT_SYSEX` direct event.
3. **Buffer Flushing:** After dispatching each chunk, `snd_seq_drain_output()` forces the kernel to flush its transmit buffer immediately.
4. **Inter-Chunk Cooldown:** The sending thread pauses for an inter-chunk cooldown delay (default **10 ms**):
   ```cpp
   if (i + chunk_len < sysex_data.size()) {
       std::this_thread::sleep_for(std::chrono::milliseconds(chunk_cooldown_ms));
   }
   ```
5. **Drain Pending Events:** Between chunks, any incoming events in the ALSA input queue are drained to prevent kernel buffer overflows.

---

### 4.10 Hardware Sequencer Pattern Protocol (`PatternData`, Function 0x4D)

The Volca Sample 2 contains an onboard 16-step, 10-part sequencer capable of storing 16 discrete patterns (slots 0–15). Each pattern encapsulates step triggers, active step states, part audio parameter assignments (speed, pitch EG, level, pan, sample slot mapping), and motion sequence automation data.

```
HOST (VolSa 2)                                      DEVICE (Volca Sample 2)
      |                                                        |
      | -------- PatternDumpRequest (F0 42 3g 00 01 2D 1D p F7) -> |
      |                                                        |
      | <------- PatternData (9,079 bytes) ------------------- |
      |          [F0 42 3g 00 01 2D 4D p <9,070 U7 bytes> F7]  |
```

#### 4.10.1 9,079-Byte SysEx Message Framing & 7,936-Byte Unpacked Binary

Sequencer pattern transfers follow the KORG Extended SysEx framing standard.

##### 1. Outgoing: `PatternDumpRequest` (9 bytes)
To query the binary sequencer state of a pattern slot, the host dispatches:
```
Byte 0..5: Extended KORG Header [F0 42 3g 00 01 2D]
Byte 6:    0x1D (Command: Pattern Dump Request)
Byte 7:    <pattern_no> (Target pattern index: 0x00 - 0x0F, masked with 0x7F)
Byte 8:    0xF7 (EOX)
```

##### 2. Incoming / Outgoing: `PatternData` (9,079 bytes)
In response to `PatternDumpRequest`, or when the host writes an updated pattern back to flash, a fixed-size 9,079-byte message is transferred:
```
Byte 0..5:     Extended KORG Header [F0 42 3g 00 01 2D]
Byte 6:        0x4D (Command: Pattern Data Dump)
Byte 7:        <pattern_no> (Pattern index: 0x00 - 0x0F)
Byte 8..9077:  [9,070 bytes: 7-bit packed payload of 7,936 raw binary bytes]
Byte 9078:     0xF7 (EOX)
```

##### Mathematical Size Derivation:
- Unpacked raw binary size: $L_{\text{in}} = 7,936\text{ bytes}$ ($0x1F00$ bytes).
- 7-bit packed payload length:
  $$L_{u8 \to u7}(7936) = 7936 + \left\lceil \frac{7936}{7} \right\rceil = 7936 + 1134 = 9,070\text{ bytes}$$
- Total SysEx envelope:
  $$L_{\text{SysEx}} = 8\text{ (Header + Function + Pattern No)} + 9,070\text{ (Payload)} + 1\text{ (EOX)} = 9,079\text{ bytes}$$

#### 4.10.2 Pattern Memory Layout: 'PTST' Magic, Name Offset 16, and Step Sequences

When unpacked via `U7ToU8`, the 7,936 bytes form the native hardware memory representation of the sequencer:

| Byte Offset | Field Name | Data Type | Description |
|---|---|---|---|
| `0x0000 .. 0x0003` | `magic` | `char[4]` | Sequence Header Signature: `'P'`, `'T'`, `'S'`, `'T'` (`0x50, 0x54, 0x53, 0x54`). |
| `0x0004 .. 0x000F` | `reserved_header` | `uint8_t[12]` | Internal sequencer hardware state, flags, tempo, and groove settings. |
| `0x0010 .. 0x003F` | `name` | `char[48]` | Pattern ASCII name string (null-terminated / null-padded, max 48 bytes). |
| `0x0040 .. 0x1EFF` | `sequence_payload`| `uint8_t[7872]` | Sequence step triggers, active steps, part assignments, motion sequence parameters. |

#### 4.10.3 In-Place Pattern Renaming Algorithm Preserving Trigger & Motion Automation

Because reverse-engineering the entirety of the 7,872 bytes of proprietary motion sequence and trigger tables is unnecessary for renaming, VolSa 2 implements an **in-place mutation algorithm**:

1. **Fetch Original Pattern:** Download the existing 9,079-byte pattern via `PatternDumpRequest` for slot index $p \in [0, 15]$.
2. **Unpack 7-Bit Encoding:** Unpack the 9,070-byte 7-bit payload using `U7ToU8::convert()` to yield the 7,936-byte raw binary representation.
3. **Validate Magic Signature:** Assert that bytes `0..3` match `'P', 'T', 'S', 'T'`.
4. **Mutate ASCII Name Buffer:**
   - Overwrite bytes `16 .. 63` with the new ASCII name string.
   - Any unused characters up to offset 63 are strictly zero-padded (`0x00`).
   - Truncate input names longer than 48 characters to maintain buffer bounds.
5. **Preserve Sequence Data:** **Keep byte 0..15 and all bytes from offset 64 to 7935 completely untouched.**
6. **Repack 7-Bit Encoding:** Re-encode the 7,936 bytes using `U8ToU7::convert()` back into exactly 9,070 7-bit bytes.
7. **Transmit SysEx to Hardware:** Dispatch the resulting 9,079-byte SysEx payload using 256-byte chunk slicing.
8. **Result:** The Volca updates the displayed pattern name in flash without altering a single trigger, velocity, step state, or motion knob curve!

---

## 5. Mathematical & Algorithmic Foundations: 7-bit MIDI Octet Packing

The 7-bit packing algorithm is the mathematical foundation of the entire communication protocol. This section provides the formal mathematical proof, bitwise assembly diagrams, and algorithmic implementations.

### 5.1 The 7-Bit Invariant: The Status Byte Collision Problem
The MIDI 1.0 physical layer protocol partitions all 8-bit bytes into two mutually exclusive sets:
$$\text{Data Bytes: } D = \{ x \in \mathbb{Z} \mid 0 \le x \le 127 \} \iff b_7 = 0$$
$$\text{Status Bytes: } S = \{ x \in \mathbb{Z} \mid 128 \le x \le 255 \} \iff b_7 = 1$$

Within a System Exclusive stream:
- Any byte with $b_7 = 1$ is interpreted by MIDI hardware as a real-time status byte (e.g. `0xF8` Clock, `0xFE` Active Sensing) or an unexpected status command.
- If an unencoded 8-bit audio sample contains a byte with $b_7 = 1$, the receiving MIDI hardware will truncate the SysEx reception immediately.
- Therefore, every byte transmitted between `0xF0` and `0xF7` must strictly belong to $D$.

### 5.2 Mathematical Formulation of 7-to-8 Octet Packing
To convey arbitrary 8-bit binary data through a 7-bit channel without losing information, we group data into blocks where the total number of bits is a multiple of both 8 and 7:
$$\operatorname{lcm}(8, 7) = 56\text{ bits}$$

A 56-bit block can be represented as:
- **Input:** Exactly 7 bytes of 8-bit data ($7 \times 8 = 56\text{ bits}$).
- **Output:** Exactly 8 bytes of 7-bit data ($8 \times 7 = 56\text{ bits}$).

Each 7-byte input slice $D = [d_0, d_1, d_2, d_3, d_4, d_5, d_6]$ is transformed into an 8-byte output slice $O = [s_0, s_1, s_2, s_3, s_4, s_5, s_6, s_7]$:
- $s_0$ (the MSB container byte) collects the most significant bit ($b_7$) of each of the 7 data bytes.
- $s_1 \dots s_7$ store the lower 7 bits ($b_0 \dots b_6$) of each respective data byte.

```
INPUT: 7 Raw 8-Bit Bytes (56 bits total)
Byte 0 (d0): [ MSB0 | b0_6 | b0_5 | b0_4 | b0_3 | b0_2 | b0_1 | b0_0 ]
Byte 1 (d1): [ MSB1 | b1_6 | b1_5 | b1_4 | b1_3 | b1_2 | b1_1 | b1_0 ]
Byte 2 (d2): [ MSB2 | b2_6 | b2_5 | b2_4 | b2_3 | b2_2 | b2_1 | b2_0 ]
Byte 3 (d3): [ MSB3 | b3_6 | b3_5 | b3_4 | b3_3 | b3_2 | b3_1 | b3_0 ]
Byte 4 (d4): [ MSB4 | b4_6 | b4_5 | b4_4 | b4_3 | b4_2 | b4_1 | b4_0 ]
Byte 5 (d5): [ MSB5 | b5_6 | b5_5 | b5_4 | b5_3 | b5_2 | b5_1 | b5_0 ]
Byte 6 (d6): [ MSB6 | b6_6 | b6_5 | b6_4 | b6_3 | b6_2 | b6_1 | b6_0 ]

OUTPUT: 8 MIDI 7-Bit Bytes (56 bits total, all b7 == 0)
s0 (Header): [  0   | MSB6 | MSB5 | MSB4 | MSB3 | MSB2 | MSB1 | MSB0 ]
s1 (Data 0): [  0   | b0_6 | b0_5 | b0_4 | b0_3 | b0_2 | b0_1 | b0_0 ]
s2 (Data 1): [  0   | b1_6 | b1_5 | b1_4 | b1_3 | b1_2 | b1_1 | b1_0 ]
s3 (Data 2): [  0   | b2_6 | b2_5 | b2_4 | b2_3 | b2_2 | b2_1 | b2_0 ]
s4 (Data 3): [  0   | b3_6 | b3_5 | b3_4 | b3_3 | b3_2 | b3_1 | b3_0 ]
s5 (Data 4): [  0   | b4_6 | b4_5 | b4_4 | b4_3 | b4_2 | b4_1 | b4_0 ]
s6 (Data 5): [  0   | b5_6 | b5_5 | b5_4 | b5_3 | b5_2 | b5_1 | b5_0 ]
s7 (Data 6): [  0   | b6_6 | b6_5 | b6_4 | b6_3 | b6_2 | b6_1 | b6_0 ]
```

---

### 5.3 Step-by-Step Bitwise Assembly Algorithm (`U8ToU7`)

For an arbitrary input buffer of length $L$, the algorithm iterates through chunks of size $k \in [1, 7]$:

```cpp
std::vector<U7> U8ToU7::convert(std::span<const uint8_t> input) {
    std::vector<U7> output;
    output.reserve(convert_len(input.size()));

    for (size_t i = 0; i < input.size(); i += 7) {
        size_t chunk_len = std::min<size_t>(7, input.size() - i);
        
        uint8_t msb_header = 0;
        // Step 1: Assemble the MSB header byte
        for (size_t j = 0; j < chunk_len; ++j) {
            uint8_t msb_bit = (input[i + j] >> 7) & 1;
            msb_header |= (msb_bit << j);
        }
        output.push_back(U7(msb_header)); // Emit header

        // Step 2: Emit the 7-bit payload bytes (lower 7 bits)
        for (size_t j = 0; j < chunk_len; ++j) {
            output.push_back(U7(input[i + j] & 0x7F));
        }
    }
    return output;
}
```

---

### 5.4 Inverse Unpacking & Bit Restoration Algorithm (`U7ToU8`)

During reception, the inverse transformation processes blocks of size $m \in [2, 8]$:
1. Byte 0 is the MSB container byte.
2. For each following data byte $j \in [0, m-2]$:
   - Extract the $j$-th bit of the container byte.
   - Shift it left by 7 positions.
   - Bitwise-OR it with the lower 7 bits of the data byte:

$$\text{recovered}[j] = (s_{j+1} \ \& \ \text{0x7F}) \ | \ \left( \left( \frac{s_0 \ \& \ (1 \ll j)}{1 \ll j} \right) \ll 7 \right)$$

```cpp
std::vector<uint8_t> U7ToU8::convert(std::span<const U7> input) {
    std::vector<uint8_t> output;
    output.reserve(convert_len(input.size()));

    size_t i = 0;
    while (i < input.size()) {
        uint8_t msb_header = input[i].value();
        ++i;

        size_t chunk_len = std::min<size_t>(7, input.size() - i);
        for (size_t j = 0; j < chunk_len; ++j) {
            uint8_t msb_flag = ((msb_header >> j) & 1) ? 0x80 : 0x00;
            uint8_t restored = (input[i + j].value() & 0x7F) | msb_flag;
            output.push_back(restored);
        }
        i += chunk_len;
    }
    return output;
}
```

---

### 5.5 Exact Mathematical Buffer Length Transformation Formulas

Given an input buffer of length $L_{\text{in}}$:

#### 1. Forward Packing Length ($L_{u8 \to u7}$):
Every chunk of up to 7 bytes produces $k + 1$ output bytes (1 header byte + $k$ data bytes).
$$L_{u8 \to u7}(L_{\text{in}}) = L_{\text{in}} + \left\lceil \frac{L_{\text{in}}}{7} \right\rceil = L_{\text{in}} + \left\lfloor \frac{L_{\text{in}} + 6}{7} \right\rfloor$$

**Example Computations:**
- $L_{\text{in}} = 0 \implies 0 + 0 = 0$ bytes.
- $L_{\text{in}} = 1 \implies 1 + \lceil 1/7 \rceil = 2$ bytes.
- $L_{\text{in}} = 7 \implies 7 + \lceil 7/7 \rceil = 8$ bytes.
- $L_{\text{in}} = 32 \text{ (Sample Header)} \implies 32 + \lceil 32/7 \rceil = 32 + 5 = 37$ bytes.
- $L_{\text{in}} = 56800 \text{ (28,400 samples)} \implies 56800 + \lceil 56800/7 \rceil = 56800 + 8115 = 64915$ bytes.

#### 2. Inverse Unpacking Length ($L_{u7 \to u8}$):
$$L_{u7 \to u8}(L_{7\text{bit}}) = \begin{cases} 0, & \text{if } L_{7\text{bit}} = 0 \\ L_{7\text{bit}} - \left\lceil \frac{L_{7\text{bit}}}{8} \right\rceil, & \text{if } L_{7\text{bit}} > 0 \end{cases}$$

---

### 5.6 Mathematical Boundary Invariants: The $8n + 1$ Invalidation Law

An important structural invariant governs valid 7-bit streams:

$$\mathbf{L_{7\text{bit}} \not\equiv 1 \pmod 8 \quad \forall \text{ valid encoded streams}}$$

#### Proof:
Suppose an encoded 7-bit buffer has length $L = 8n + 1$ for some integer $n \ge 0$.
1. The first $8n$ bytes consist of $n$ complete blocks, each containing 1 header byte and 7 data bytes.
2. The final remaining byte at index $8n$ would have to be an MSB container byte.
3. However, since the total length is $8n + 1$, there are zero following data bytes to accompany this header.
4. An MSB container byte without at least one following data byte is meaningless and cannot be produced by any valid input chunk ($1 \le k \le 7$).
5. Therefore, any buffer with length $L \equiv 1 \pmod 8$ is malformed and must be rejected.

---

## 6. Digital Signal Processing (DSP) & Audio Transformation Engine

Arbitrary user audio files (MP3, FLAC, AIFF, high-resolution WAV) must pass through a multi-stage DSP pipeline to conform to the Volca Sample 2's native hardware format (31,250 Hz, 16-bit signed linear mono PCM).

```
+-----------------------------------------------------------------------------------+
|                            VOLSA 2 DSP AUDIO PIPELINE                             |
+-----------------------------------------------------------------------------------+
|  1. INPUT FILE DECODING (libsndfile)                                              |
|     - Inspect container format, sample rate (Fs), bit depth, and channel count.   |
|     - Stream interleaved audio frames into double-precision float [-1.0, +1.0].   |
|                                       │                                           |
|                                       ▼                                           |
|  2. CHANNEL DOWNMIXING MATRIX (Mid, Left, Right, Side)                            |
|     - Mid  : y[n] = 0.5 * (Left[n] + Right[n])  [Phantom center preserved]        |
|     - Left : y[n] = Left[n]                     [Left channel isolated]          |
|     - Right: y[n] = Right[n]                    [Right channel isolated]         |
|     - Side : y[n] = 0.5 * (Left[n] - Right[n])  [Stereo difference isolated]     |
|                                       │                                           |
|                                       ▼                                           |
|  3. BAND-LIMITED SINC POLYPHASE RESAMPLING (libsamplerate)                        |
|     - Quality Mode: SRC_SINC_BEST_QUALITY (~97 dB SNR, passband ripple < 0.001dB)|
|     - Resampling Ratio: ρ = 31250.0 / Fs_in                                       |
|     - Anti-Aliasing Low-Pass Filter: fc = min(Fs_in / 2, 31250 / 2)               |
|                                       │                                           |
|                                       ▼                                           |
|  4. QUANTIZATION & CLAMPING                                                       |
|     - Scale: s[n] = round(y[n] * 32767.0)                                         |
|     - Hard Clamping: clamp(s[n], -32768, 32767)                                   |
|                                       │                                           |
|                                       ▼                                           |
|  5. OUTPUT BUFFER                                                                 |
|     - Standardized std::vector<int16_t> ready for U8ToU7 packing or WAV export.   |
+-----------------------------------------------------------------------------------+
```

### 6.1 Ingestion & Multi-Codec Audio Decoding (`libsndfile`)
Audio ingestion is handled by **libsndfile** (`sndfile.h`). By utilizing `sf_readf_double()`, audio frames are normalized to 64-bit IEEE double-precision floats $[-1.0, +1.0]$. This ensures subsequent channel arithmetic and resampling calculations do not introduce quantization noise or rounding errors.

Supported container formats include:
- Microsoft RIFF WAVE (`.wav`)
- Free Lossless Audio Codec (`.flac`)
- Apple AIFF / AIFC (`.aiff`, `.aif`)
- Ogg Vorbis (`.ogg`)
- MPEG Audio Layer III (`.mp3`)
- Sun/NeXT Audio (`.au`, `.snd`)

---

### 6.2 Multi-Channel Downmixing Matrices (Mid, Left, Right, Side)
Because the Volca Sample 2 is strictly monaural, multi-channel audio must be reduced to a single channel. VolSa 2 provides four downmixing modes:

```cpp
enum class MonoMode {
    Mid,   // (Left + Right) / 2
    Left,  // Left channel only
    Right, // Right channel only
    Side   // (Left - Right) / 2
};
```

#### Mathematical Formulas & Acoustical Characteristics:

1. **Mid Mode (Default Center Mix):**
   $$y[n] = \frac{L[n] + R[n]}{2}$$
   - Preserves correlated, in-phase audio components (lead vocals, kick drums, snare drums, basslines).
   - Attenuates out-of-phase ambient content by $-3\text{ dB}$ to $-6\text{ dB}$.

2. **Left Channel Mode:**
   $$y[n] = L[n]$$
   - Isolates the left channel; completely rejects the right channel.

3. **Right Channel Mode:**
   $$y[n] = R[n]$$
   - Isolates the right channel; completely rejects the left channel.

4. **Side Mode (Stereo Difference):**
   $$y[n] = \frac{L[n] - R[n]}{2}$$
   - Cancels correlated center signals entirely ($L[n] - R[n] = 0$).
   - Isolates wide stereo ambient components (reverb tails, chorus modulation, panned synths).

---

### 6.3 Band-Limited Sinc Polyphase Resampling (`libsamplerate`)

When the source audio sample rate differs from 31,250 Hz (e.g. 44.1 kHz, 48 kHz, 96 kHz), sample rate conversion is required.

#### 6.3.1 Mathematical Principles of Band-Limited Interpolation
According to the **Nyquist-Shannon Sampling Theorem**, an analog continuous-time signal $x(t)$ sampled at frequency $F_s$ can be reconstructed without distortion if and only if it contains no frequency components above the Nyquist frequency $F_{\text{Nyquist}} = F_s / 2$.

Naive linear interpolation produces severe high-frequency imaging and harmonic distortion on transient-rich percussion:

```
Naive Linear Interpolation:
      x[n]           x[n+1]
        o             o
         \           /
          \         /
           \   x   /      <-- Sharp corners introduce artificial high-frequency harmonics!
            \ / \ /
             o   o
```

True band-limited reconstruction convolves discrete samples with an ideal sinc kernel:
$$x(t) = \sum_{n=-\infty}^{+\infty} x[n] \cdot \operatorname{sinc}\left( \frac{t - n T}{T} \right), \quad \text{where } \operatorname{sinc}(u) = \frac{\sin(\pi u)}{\pi u}$$

#### 6.3.2 Nyquist Filter Design & Anti-Aliasing Cutoff
When downsampling from $F_{\text{in}} = 44,100\text{ Hz}$ to $F_{\text{out}} = 31,250\text{ Hz}$, any frequency component above the target Nyquist limit:
$$f_{\text{target Nyquist}} = \frac{31250}{2} = 15,625\text{ Hz}$$
would fold back into the audible band as severe aliasing distortion.

VolSa 2 employs **libsamplerate** configured with `SRC_SINC_BEST_QUALITY`:
- **Resampling Ratio:**
  $$\rho = \frac{F_{\text{out}}}{F_{\text{in}}} = \frac{31250.0}{F_{\text{in}}}$$
- **Anti-Aliasing Filter:** Automatically applies an optimal polyphase sinc filter with cutoff at $\min(F_{\text{in}}/2, 31250/2)$.
- **Filter Performance:** Signal-to-Noise Ratio (SNR) $> 97\text{ dB}$, passband ripple $< 0.001\text{ dB}$, stopband attenuation $> 90\text{ dB}$.

If the input audio is already at 31,250 Hz, the resampler is bypassed entirely, preserving zero-overhead sample fidelity.

---

### 6.4 Clamping, Dynamic Range Scaling, and Quantization
Following resampling, floating-point samples $y \in [-1.0, 1.0]$ are quantized to 16-bit signed integers:

$$s[n] = \operatorname{clamp}\left( \operatorname{round}\left( y[n] \times 32767.0 \right), -32768, 32767 \right)$$

Explicit clamping prevents integer overflow and destructive fold-back clipping when hot transients exceed $0\text{ dBFS}$.

---

### 6.5 RIFF/WAVE Format Serialization Engine (44-Byte Container)
When exporting samples to disk, VolSa 2 writes a standard 44-byte RIFF WAVE header followed by raw PCM data:

```
Offset  Size  Field Name        Binary Value / Formula
0       4     ChunkID           "RIFF" (0x52494646)
4       4     ChunkSize         36 + (num_samples * 2)
8       4     Format            "WAVE" (0x57415645)
12      4     Subchunk1ID       "fmt " (0x666D7420)
16      4     Subchunk1Size     16 (PCM chunk size)
20      2     AudioFormat       1 (Linear PCM)
22      2     NumChannels       1 (Monaural)
24      4     SampleRate        31250 (Native Hz)
28      4     ByteRate          62500 (31250 * 1 * 2 bytes/sec)
32      2     BlockAlign        2 (1 channel * 16 bits / 8)
34      2     BitsPerSample     16 (Bits per sample)
36      4     Subchunk2ID       "data" (0x64617461)
40      4     Subchunk2Size     num_samples * 2
44      ...   PCM Data          Interleaved 16-bit signed LE samples
```

---

### 6.6 Audio Slicing & Transient Chopper DSP Engine (`volsa2::SampleChopper`)

For rhythm production and MPC-style workflow, VolSa 2 includes an integrated sample slicing and transient chopping subsystem (`volsa2::crop_audio`, `volsa2::find_silence_bounds`, `volsa2::divide_equal_slices`, and `volsa2::detect_transient_slices`).

#### 6.6.1 Short-Time Energy & Spectral Flux Transient Detection

Automated onset detection identifies sudden bursts of acoustic energy characteristic of percussion attacks (kicks, snares, rimshots, synth plucks).

##### Mathematical Energy Formulation:
1. The PCM audio buffer $x[n] \in [-32768, 32767]$ is normalized and partitioned into non-overlapping blocks of size $B = 128$ samples (~4.096 ms at 31,250 Hz).
2. The Short-Time Energy (STE) for each block $b$ is computed:
   $$E[b] = \frac{1}{B} \sum_{i=0}^{B-1} \left( \frac{x[b \cdot B + i]}{32768.0} \right)^2$$
3. The peak energy across the signal is tracked: $E_{\max} = \max_b E[b]$.
4. The forward energy difference (energy flux novelty curve) measures sudden increases in volume:
   $$\Delta E[b] = E[b] - E[b - 1]$$
5. Given a user sensitivity parameter $\gamma \in [0.05, 0.95]$ (default $\gamma = 0.5$):
   $$\Theta_{\text{onset}} = E_{\max} \cdot (1.0 - \gamma) \cdot 0.25$$
6. An onset boundary is declared at sample $n = b \cdot B$ if:
   $$\Delta E[b] > \Theta_{\text{onset}} \quad \text{and} \quad (b - b_{\text{last}}) \ge D_{\min}$$
   where $D_{\min} = \max(4, \frac{F_s / 20}{B}) \approx 50\text{ ms}$ enforces a minimum refractory distance to avoid spurious duplicate triggers during cymbal decays or ringing snares.

#### 6.6.2 Equal-Division & Beat/Grid Subdivision Models

For breaks, loops, and tempo-synced stems, `divide_equal_slices()` partitions an audio file into $N$ equal parts without gaps:
$$\text{start}_i = \left\lfloor \frac{i \cdot L_{\text{total}}}{N} \right\rfloor, \quad \text{end}_i = \left\lfloor \frac{(i + 1) \cdot L_{\text{total}}}{N} \right\rfloor \quad \forall i \in [0, N - 1]$$
- Guarantees $\text{start}_0 = 0$ and $\text{end}_{N-1} = L_{\text{total}}$.
- Guarantees $\text{end}_i = \text{start}_{i+1}$ with zero omitted samples.

#### 6.6.3 Zero-Crossing Snapping Algorithm to Eliminate Boundary Artifacts

When an audio waveform is sliced at an arbitrary sample position, the amplitude $x[n]$ is rarely zero. Truncating playback abruptly causes a sharp vertical discontinuity ($\Delta y = x[n]$), producing an audible, high-frequency "click" or "pop" transient.

VolSa 2 applies a **dual anti-click strategy**:
1. **Zero-Crossing Alignment:** Bound boundaries scan locally within a $\pm 32$ sample window to find the closest point where $x[n] \cdot x[n+1] \le 0$ and $|x[n]|$ is minimized.
2. **Half-Cosine Micro-Fade (`apply_edge_fade`):** For slices exceeding 64 samples, a 32-sample half-cosine envelope is applied to both the head and tail of the slice:
   $$w_{\text{in}}[i] = \frac{1 - \cos\left( \frac{\pi \cdot i}{L_{\text{fade}}} \right)}{2}, \quad w_{\text{out}}[i] = \frac{1 - \cos\left( \frac{\pi \cdot i}{L_{\text{fade}}} \right)}{2}$$
   where $L_{\text{fade}} = \min(32, \lfloor L_{\text{slice}} / 4 \rfloor)$. This attenuates any remaining DC offset smoothly to zero within 1 millisecond.

#### 6.6.4 Noise-Floor Envelope Silence Auto-Trimming

The `find_silence_bounds()` algorithm inspects audio blocks ($B = 64$) against a power threshold derived from a user-specified decibel level $\text{dB}_{\text{thresh}}$ (default $-48\text{ dBFS}$):
$$P_{\text{thresh}} = \left( 32768.0 \cdot 10^{\frac{\text{dB}_{\text{thresh}}}{20.0}} \right)^2$$
- **Forward Attack Scan:** Scans from sample index 0 forward in blocks of 64. The start boundary is declared at the first block whose RMS power exceeds $P_{\text{thresh}}$.
- **Reverse Release Scan:** Scans backward from the file end. The release boundary is declared at the last block exceeding $P_{\text{thresh}}$.
- Calling `auto_trim_silence()` strips dead air, latency delays, and room noise before upload.

#### 6.6.5 Slice Target Slot Management & Auto-Advancing Allocation

Unlike naive editors that overwrite the original sample slot or dump slices into random slots, VolSa 2 provides granular, interactive destination assignment:
- Each detected slice displays its target slot index in an editable, badged spinbox.
- The UI displays real-time hardware slot status (e.g. `[Occupied: KICK_01]` in amber vs `[Empty]` in green).
- Selecting a starting base slot (e.g. Slot 30) automatically cascades sequential slice targets (Slice 0 $\to$ 30, Slice 1 $\to$ 31, Slice 2 $\to$ 32, etc.) while alerting the user if any proposed destination would overwrite an existing on-board sample.

---

## 7. KORG Preset Package Archive Specification (`.ivlcsplpreset`)

To archive, back up, and share entire machine states (including all 16 sequencer patterns and up to 200 samples), VolSa 2 implements native reading and writing of KORG's official `.ivlcsplpreset` package format.

### 7.1 Official KORG ZIP Container Hierarchy & File Topology

An `.ivlcsplpreset` file is a standard PKZIP archive structured according to KORG's Sound Librarian specification:

```
my_preset.ivlcsplpreset  (PKZIP Archive)
│
├── FileInformation.xml            [Manifest indexing all programs & samples]
├── PresetInformation.xml          [Dataset author, title, version, date]
│
├── Prog_000.prog_info             [Pattern 1 XML metadata & MD5 checksum]
├── Prog_000.prog_bin              [Pattern 1 7,936-byte raw sequencer binary]
├── ...
├── Prog_015.prog_info             [Pattern 16 XML metadata & MD5 checksum]
├── Prog_015.prog_bin              [Pattern 16 7,936-byte raw sequencer binary]
│
├── Smpl_000.smpl_info             [Sample 000 XML metadata & MD5 checksum]
├── Smpl_000.smpl_bin              [Sample 000 raw 16-bit LE PCM binary]
├── ...
└── Smpl_199.smpl_info             [Sample 199 XML metadata & MD5 checksum]
```

### 7.2 XML Manifest Metadata Schemas

#### 1. Manifest Index: `FileInformation.xml`
Declares the target device model and binds each numbered program and sample entry to its respective files:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<KorgMSLibrarian_Data>
  <Product>volca sample 2</Product>
  <Contents NumProgramData="16" NumSampleData="200" NumPresetInformation="1">
    <PresetInformation>
      <File>PresetInformation.xml</File>
    </PresetInformation>
    <ProgramData>
      <Information>Prog_000.prog_info</Information>
      <ProgramBinary>Prog_000.prog_bin</ProgramBinary>
    </ProgramData>
    <!-- ... entries for Prog_001 through Prog_015 ... -->
    <SampleData>
      <Information>Smpl_000.smpl_info</Information>
      <SampleBinary>Smpl_000.smpl_bin</SampleBinary>
    </SampleData>
    <!-- ... entries for Smpl_001 through Smpl_199 ... -->
  </Contents>
</KorgMSLibrarian_Data>
```
*Note:* If a sample slot is vacant, its `<SampleData>` node contains only `<Information>`, omitting `<SampleBinary>` to conserve disk space.

#### 2. Preset Metadata: `PresetInformation.xml`
```xml
<?xml version="1.0" encoding="UTF-8"?>
<vlcspl2_Preset>
  <DataID>Techno Essentials Vol 1</DataID>
  <Name>Techno Essentials Vol 1</Name>
  <Author>Audio Engineer</Author>
  <Version>1</Version>
  <NumPrograms>16</NumPrograms>
  <NumSamples>200</NumSamples>
  <Date>10-09-2026</Date>
  <Prefix></Prefix>
  <Copyright>© 2026</Copyright>
</vlcspl2_Preset>
```

#### 3. Program Descriptor: `Prog_XXX.prog_info`
```xml
<?xml version="1.0" encoding="UTF-8"?>
<ProgramInformation>
  <Name>Acid Groove 01</Name>
  <Comment>Main Lead Pattern</Comment>
  <MD5>a1b2c3d4e5f60718293a4b5c6d7e8f90</MD5>
</ProgramInformation>
```

#### 4. Sample Descriptor: `Smpl_XXX.smpl_info`
```xml
<?xml version="1.0" encoding="UTF-8"?>
<SampleInformation>
  <Name>909_KICK</Name>
  <Length>14200</Length>
  <SampleRate>31250</SampleRate>
  <Channels>1</Channels>
  <BitDepth>16</BitDepth>
  <Speed>16384</Speed>
  <Level>65535</Level>
  <Author>Sound Designer</Author>
  <Comment>Punched 909 kick</Comment>
  <MD5>e4d909c290d0fb1ca068ffaddf22cbd0</MD5>
</SampleInformation>
```

### 7.3 Program Sequencer Archival (`programs/`)
All 16 patterns are archived as exact 7,936-byte raw binaries in `Prog_XXX.prog_bin`. Each binary starts with the `'PTST'` signature, preserves pattern names at offset 16, and retains all step motion lanes and sequence triggers.

### 7.4 Sample Descriptors & 31,250 Hz PCM Serialization (`samples/`)
Audio data in `Smpl_XXX.smpl_bin` is stored as pure uncompressed 16-bit Little-Endian mono linear PCM samples at 31,250 Hz. MD5 checksums are computed over the uncompressed binary stream via OpenSSL (`compute_md5_hex()`) to guarantee data integrity across extraction cycles.

### 7.5 The Flash Sector Exhaustion Problem: Allocation Mechanics & Sector Reclaim

The Volca Sample 2 flash memory contains **8,192 physical sectors** (~4 MB total, equivalent to ~65.5 seconds of audio). Sample sectors are allocated dynamically upon write and freed upon erasure.

#### The Failure Scenario:
Consider a user whose Volca Sample 2 already contains 90 large samples occupying 7,200 out of 8,192 sectors (~88% full).
- The user attempts to upload a new preset package containing 24 samples requiring 2,500 sectors.
- Naive upload behavior writes package slots $0 \dots 23$ sequentially.
- However, prior to slot 0 being overwritten, the device requires free flash sectors to buffer incoming data.
- Furthermore, slots $24 \dots 89$ from the old session remain untouched in hardware memory, holding over 5,000 sectors of obsolete audio.
- As the upload proceeds, the Volca exhausts its free sector pool and returns a fatal negative acknowledgment:
  ```
  NakStatus::SampleFull (0x25)
  ```
The upload terminates prematurely, leaving the hardware in an inconsistent state.

### 7.6 The Pre-Clean Reclaim Algorithm (Phase 1 Flash Cleaning Prior to Audio Transmission)

To eliminate false `SampleFull` aborts, VolSa 2 implements a **Two-Phase Pre-Clean Reclaim Algorithm** during package restoration:

```
[START PACKAGE UPLOAD]
         │
         ▼
[Phase 1: Pre-Clean Sector Reclaim]
 ├── 1. Scan package manifest: identify which slots [0..199] are ACTIVE in package.
 ├── 2. Query hardware: fetch headers for all slots [0..199] currently on device.
 └── 3. Identify obsolete hardware slots:
        For each slot i in 0..199:
           if (device_slot[i].is_occupied() && !package_slot[i].is_active()):
               Emit: "Freeing memory: erasing old slot i..."
               Transmit: SampleHeader::empty(i) [F0 42 3g 00 01 2D 4E ... F7]
               Await: ACK_STATUS (0x23)
         │
         ▼
[Flash Memory Pool Maximized: Obsolete sectors returned to hardware free list]
         │
         ▼
[Phase 2: Sequential Ingestion]
 ├── 4. Upload active sample headers & PCM binary dumps (slots 0..N).
 └── 5. Upload 16 sequencer patterns (slots 0..15).
         │
         ▼
[UPLOAD COMPLETE - 100% Deterministic Success]
```

By guaranteeing that any hardware slots marked empty in the incoming package are wiped **before** transmitting new audio binaries, the device flash memory is maximized, allowing even massive preset packs to install reliably without encountering `SampleFull` errors.

---

## 8. Architectural Translation: Rust vs. Modern C++20

The original `volsa2` was written in Rust. The table below details the translation strategies employed during the port to modern C++20:

| Architectural Dimension | Original Rust (`volsa2`) | Modern C++20 (`volsa2-cpp`) | Engineering Rationale |
|---|---|---|---|
| **Non-Owning Views** | Slices (`&[u8]`) | `std::span<const uint8_t>` | Bounds-safe, non-allocating memory views matching Rust slice semantics. |
| **Optional Values** | `Option<T>` | `std::optional<T>` | Expressive zero-overhead optionality for value-type validation (`U7::checked`). |
| **Endianness** | `byteorder` crate (`LittleEndian`) | C++20 `<bit>` (`std::endian`) & explicit bit shifts | Deterministic cross-platform byte order handling without third-party crates. |
| **Error Handling** | `Result<T, E>`, `anyhow`, `thiserror` | C++ Exception Hierarchy (`std::runtime_error`) | Clean separation of business logic from error recovery; aligns with Qt signal propagation. |
| **Audio File I/O** | `hound` crate (WAV only) | `libsndfile` (`sndfile.h`) | Broadens format support to FLAC, AIFF, OGG, and floating-point audio. |
| **Resampling Engine** | `rubato` crate (`FftFixedIn`) | `libsamplerate` (Secret Rabbit Code) | Gold-standard, SIMD-accelerated polyphase sinc interpolation. |
| **MIDI Subsystem** | `alsa` crate (`alsa::seq`) | Native Linux ALSA `asoundlib.h` | Direct C ABI integration; avoids unsafe FFI wrappers and reduces memory overhead. |
| **CLI Argument Parser** | `clap` crate with derive macros | Custom clean argument parser (`cli/main.cpp`) | Zero external dependency footprint; supports identical flags and interactive prompts. |
| **Concurrency / Worker** | Synchronous single-threaded | Multi-threaded (`QThread` + `DeviceWorker`) | Prevents GUI freezing during multi-second SysEx sample transfers. |
| **Audio Playback** | No audio playback support | Native ALSA PCM Engine (`AlsaAudioPlayer`) | High-performance audition engine with zero desktop framework dependencies. |

### 8.1 Memory Safety: Rust Borrow Checker vs. `std::span` Views
Rust enforces memory safety via compile-time lifetime annotations and borrow-checking rules. In C++20, similar non-allocating, zero-copy safety is achieved using `std::span`:

```cpp
// C++20: Zero-copy span view over an existing buffer
void process_packet(std::span<const uint8_t> packet) {
    if (packet.size() < 6) return;
    auto header = packet.subspan(0, 6);
    auto payload = packet.subspan(6);
    // Bounds-checked, non-allocating sub-views
}
```

### 8.2 Monadic Error Handling vs. C++ Exception Hierarchies
Rust propagates errors using `Result<T, E>` and the `?` operator. In `volsa2-cpp`, errors are classified into clear exception types:
- `volsa2::DeviceError`: Base class for all hardware and protocol exceptions.
- `volsa2::TimeoutError`: Raised when the hardware fails to respond within the expected window.
- `volsa2::NakError`: Raised when the hardware returns a negative acknowledge (`Busy`, `SampleFull`, `DataFormat`).
- `volsa2::ProtocolError`: Raised when packets violate SysEx framing or length rules.

This maps directly to Qt's signal/slot architecture: the worker thread catches exceptions and emits an `errorOccurred(QString)` signal to update the UI cleanly.

### 8.3 Value Semantics & Compile-Time Safety
Modern C++20 leverages strongly-typed value wrappers such as `U7` (`seven_bit.hpp`) and structured bindings. By encapsulating 7-bit MIDI invariants inside a lightweight value class with `constexpr` assertions, invalid byte configurations are caught at compile-time or via zero-cost bounds checking rather than causing silent corruption in ALSA driver pipelines.

### 8.4 C ABI Direct Integration vs. Rust FFI Bindings
While Rust relies on `bindgen` and unsafe extern "C" blocks to interact with the Linux ALSA sequencer (`libasound.so`), C++20 includes `<alsa/asoundlib.h>` directly. Direct C ABI integration eliminates marshalling overhead, removes unsafe pointer indirection layers, and provides native RAII wrapping around ALSA pointer handles (`snd_seq_t*`, `snd_pcm_t*`).

### 8.5 Concurrency Models: Single-Threaded CLI vs. Multi-Threaded Qt
The CLI operates strictly single-threaded with synchronous timeouts, matching the deterministic nature of shell scripts. In contrast, the GUI architecture introduces asynchronous multi-threading via `QThread` and queued signal/slot connections, ensuring that high-latency MIDI transfers never preempt UI rendering or audio preview streaming.

---

## 9. Qt 6 Graphical User Interface Architecture

The desktop application (`volsa2-gui`) provides a modern, responsive graphical librarian and workstation for managing the Volca Sample 2.

```
+---------------------------------------------------------------------------------------------------+
| MainWindow (UI Thread)                                                                            |
|                                                                                                   |
|  +---------------------------------------------------------------------------------------------+  |
|  | [LED] Connected (v1.02, Ch 1)  [Connect] [Refresh All]   Space: [======== 66.2% ========]   |  |
|  +---------------------------------------------------------------------------------------------+  |
|  | [Upload Sample...] [Upload Package...] [Download Package...] [Chop / Slice...]               |  |
|  +---------------------------------------------------------------------------------------------+  |
|  |  TAB 1: Samples (200 Slots)              |  TAB 2: Sequencer Patterns (16 Slots)            |  |
|  |  +-------------------------------------+  |  +-------------------------------------------+  |  |
|  |  | [Download (3)...] [Erase (3)...]    |  |  | Slot | Pattern Name          | Status     |  |  |
|  |  | Slot | Name      | Length | Status  |  |  | 01   | Funky Break 01        | Occupied   |  |  |
|  |  | 000* | KICK_01   | 12450  | Occupied|  |  | 02   | Acid Bassline         | Occupied   |  |  |
|  |  | 001* | SNARE_909 | 18200  | Occupied|  |  | ...  | ...                   | ...        |  |  |
|  |  | 002* | HIHAT_CL  |  6800  | Occupied|  |  | [Rename Selected Pattern...]               |  |  |
|  |  | 003  | <EMPTY>   |     0  | Empty   |  |  +-------------------------------------------+  |  |
|  |  +-------------------------------------+                                                     |  |
|  +---------------------------------------------------------------------------------------------+  |
|  | WaveformWidget (Peak-decimated canvas, interactive seekbar, playhead)                       |  |
|  +---------------------------------------------------------------------------------------------+  |
|  | [▶ Play] [■ Stop]  Slot 000: "KICK_01" (12450 smpls, 0.40s)    Vol: [==========O======]     |  |
|  +---------------------------------------------------------------------------------------------+  |
+-------------------------------------------------^-------------------------------------------------+
                                                  | Qt::QueuedConnection Signals / Slots
+-------------------------------------------------v-------------------------------------------------+
| DeviceWorker (Secondary QThread)                                                                  |
|  - Owned volsa2::Device instance                                                                  |
|  - Executes blocking ALSA sequencer I/O without stuttering the UI                                 |
|  - Dispatches: slotLoaded(), patternsLoaded(), sampleDataReady(), progress(), spaceUpdated()      |
+---------------------------------------------------------------------------------------------------+
```

### 9.1 Non-Blocking Concurrency: The `DeviceWorker` / `QThread` Pattern
Querying 200 slots, fetching 9,079-byte pattern dumps, or transferring large sample packages requires tens of seconds of blocking ALSA I/O. Executing these operations on the main UI thread would freeze the event loop and trigger OS "Application Not Responding" (ANR) hangs.

`DeviceWorker` inherits from `QObject` and is moved to an independent worker `QThread`:
```cpp
worker_ = new DeviceWorker();
worker_->moveToThread(&worker_thread_);

connect(&worker_thread_, &QThread::finished, worker_, &QObject::deleteLater);
connect(this, &MainWindow::requestConnect, worker_, &DeviceWorker::connectDevice);
connect(worker_, &DeviceWorker::deviceConnected, this, &MainWindow::onDeviceConnected);
connect(worker_, &DeviceWorker::progress, this, &MainWindow::onProgress);
// All communication crosses thread boundaries via Qt::QueuedConnection
worker_thread_.start();
```

### 9.2 Thread-Safe Inter-Thread Signal/Slot Marshalling & Meta-Types
To safely pass domain models across thread boundaries without race conditions, custom types are registered with the Qt Meta-Object system:
```cpp
qRegisterMetaType<volsa2::SampleHeader>("volsa2::SampleHeader");
qRegisterMetaType<std::vector<volsa2::SampleHeader>>("std::vector<volsa2::SampleHeader>");
qRegisterMetaType<volsa2::PatternData>("volsa2::PatternData");
qRegisterMetaType<std::vector<volsa2::PatternData>>("std::vector<volsa2::PatternData>");
qRegisterMetaType<std::vector<int16_t>>("std::vector<int16_t>");
```

### 9.3 Tab-Widget Hierarchical Architecture (Samples View vs. Patterns View)
The primary UI layout is organized as a `QTabWidget` with two dedicated views:
1. **Samples Tab:** Hosts the 200-slot sample table view, multi-sample batch actions, filtering, search, and audio waveform inspector.
2. **Patterns Tab:** Hosts the 16-slot sequencer pattern manager, displaying pattern numbers, names, lengths, and direct renaming triggers.

Both tabs share a global top header containing the connection status LED, hardware channel/firmware indicators, the "Refresh All" button, and the global flash memory progress bar.

### 9.4 Interactive Sequencer Pattern Manager & Hardware Renaming UI
Selecting a pattern row in the Patterns tab enables pattern management:
- **Refresh Patterns:** Dispatches `fetchPatterns()` to query slots 0–15 via `PatternDumpRequest` (0x1D).
- **Rename Pattern:** Prompts the user with a `QInputDialog` to enter a new pattern title (up to 48 characters).
- Upon confirmation, `DeviceWorker::renamePattern(slot, newName)` executes the in-place renaming algorithm: it downloads the 9,079-byte binary, asserts `'PTST'`, modifies bytes 16–63 with the new string, repacks the 7-bit octets, and uploads the modified SysEx packet back to flash.

### 9.5 Multi-Sample Extended Selection Mode (`QAbstractItemView::ExtendedSelection`)
The sample table view (`sample_table_`) is configured with:
```cpp
sample_table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
sample_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
```
This enables full desktop selection paradigms:
- **Click:** Selects a single slot.
- **Ctrl + Click:** Toggles individual slots in/out of the selection set.
- **Shift + Click:** Selects a contiguous range of slots.
- **Rubber-band Drag:** Selects all touched rows.

The helper method `MainWindow::selectedSlots()` iterates over `sample_table_->selectionModel()->selectedRows()` to extract a clean, deduplicated vector of selected slot indices (`std::vector<uint8_t>`).

### 9.6 Multi-Slot Batch Operations: Batch Download, Batch Erase, and Context Menu Helpers
When multiple slots are selected, action buttons dynamically update their labels to reflect the active selection count:
- **Dynamic Button Labels:** "Download Selected (4)...", "Erase Selected (4)...".
- **Batch Download:** Prompts the user to select a destination directory (`QFileDialog::getExistingDirectory`). The worker iterates through the selected slots, queries each audio dump, normalizes the filename to `<slot>_<name>.wav`, and writes the 44-byte RIFF container.
- **Batch Erase:** Presents a safety confirmation showing the exact slots to be wiped. Upon user confirmation, `DeviceWorker::eraseSamples()` transmits `SampleHeader::empty()` for each slot sequentially.
- **Context Submenu:** Right-clicking the table presents quick selection helpers:
  - *"Select All Occupied"*: Scans cached headers and selects every slot with `length > 0`.
  - *"Invert Selection"*: Inverts the current row selection mask.
  - *"Clear Selection"*: Deselects all rows.

### 9.7 Multi-File Drag-and-Drop Ingestion Engine
The sample table view intercepts Qt drag-and-drop events:
1. `dragEnterEvent` & `dragMoveEvent`: Checks `event->mimeData()->hasUrls()` and accepts if audio files (`.wav`, `.aif`, `.flac`, `.mp3`) are present.
2. `dropEvent`: Extracts file paths. If multiple files are dropped:
   - Identifies the target slot under the mouse cursor.
   - Sequential files are assigned to consecutive slots starting from the drop target.
   - For each file, the conversion pipeline resamples audio to 31,250 Hz mono and streams it to the hardware via `DeviceWorker::uploadSample()`.

### 9.8 Interactive Waveform Visualizer (`WaveformWidget` & Peak Decimation)
`WaveformWidget` renders audio waveforms using hardware-accelerated `QPainter`.

#### Peak Decimation Algorithm:
Drawing 100,000+ samples as individual vectors would overwhelm 2D rasterization. `WaveformWidget` employs **peak decimation**:
1. The audio buffer is partitioned into $W$ horizontal slices (where $W$ is the widget width in pixels).
2. For each pixel column, the minimum and maximum sample values within that slice are computed.
3. A single vertical bar spanning from `min` to `max` is rendered.
This allows rendering 100,000+ samples instantaneously at 60 FPS.

```cpp
void WaveformWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    int w = width();
    int h = height();
    int mid_y = h / 2;

    double samples_per_pixel = static_cast<double>(samples_.size()) / w;

    for (int x = 0; x < w; ++x) {
        size_t start_idx = static_cast<size_t>(x * samples_per_pixel);
        size_t end_idx = std::min(samples_.size(), static_cast<size_t>((x + 1) * samples_per_pixel));

        int16_t min_val = 0;
        int16_t max_val = 0;
        for (size_t i = start_idx; i < end_idx; ++i) {
            min_val = std::min(min_val, samples_[i]);
            max_val = std::max(max_val, samples_[i]);
        }

        int y_top = mid_y - (max_val * mid_y / 32768);
        int y_bot = mid_y - (min_val * mid_y / 32768);
        painter.drawLine(x, y_top, x, y_bot);
    }
}
```

### 9.9 Interactive Sample Chopper Dialog (`SampleChopperDialog`) with Live Target Status
Launched via the "Chop / Slice..." button or context menu:
- **Slice Modes:** Supports both Equal Grid (2, 4, 8, 16, 32 slices) and Transient Detection (energy flux with sensitivity slider).
- **Auto-Trim Silence:** RMS power slider (-60 dB to -12 dB) strips leading and trailing silence.
- **Anti-Click Fades:** Optional 32-sample half-cosine boundary smoothing.
- **Live Target Status:** Each slice table row features an editable spinbox for its destination hardware slot, accompanied by a dynamic colored badge:
  - `[Empty]` (Green): Slot is unallocated and safe to write.
  - `[Occupied: NAME]` (Amber): Slot contains an existing sample that will be overwritten.
- **Base Slot Quick-Assign:** Selecting a starting base slot automatically advances subsequent slices to consecutive slots ($S_0 \to \text{base}, S_1 \to \text{base}+1, \dots$).
- **Export Pipeline:** Clicking "Export Slices to Device" dispatches slice buffers to the worker thread for sequential hardware writing.

### 9.10 Preset Package Download & Restore Wizards
- **Download Package Wizard:** Allows the user to archive the device's state to an `.ivlcsplpreset` file. The wizard offers checkboxes to include sequencer patterns, sample descriptors, and raw audio binaries.
- **Upload Package Wizard:** Guides the user through restoring an `.ivlcsplpreset` package. Provides options for:
  - "Upload Samples" (Phase 1 pre-clean + Phase 2 upload)
  - "Upload Patterns" (Restores 16 sequencer pattern programs)
  - "Erase unused device slots" (Enables Phase 1 sector reclaim to eliminate `SampleFull` errors).

---

## 10. Field Debugging & Real-World Post-Mortem

During the testing, hardware verification, and field integration phases, several complex protocol, kernel, and compiler traps were uncovered and resolved. This section documents their root causes and architectural resolutions.

### 10.1 The Infamous "No space left on device" (-ENOSPC) ALSA Bug

#### Symptom:
When uploading or selecting samples, the application would intermittently fail with:
```
Failed to upload sample to slot 94: snd_seq_event_input failed: No space left on device
Failed to fetch sample 14: snd_seq_event_input failed: No space left on device
```
This error occurred even when the Volca's internal flash memory was reported as 70% free.

#### Root-Cause Analysis:
The error string `"No space left on device"` corresponds to standard POSIX error code **`ENOSPC` (errno 28)**. Users frequently misinterpret this as a lack of physical flash memory on the Volca. In reality, the error was being generated by the **Linux kernel ALSA Sequencer driver**:

1. The ALSA sequencer allocates an internal ring buffer of event cells for each client. By default, this pool is restricted to just 200 event cells.
2. When external MIDI hardware transmits continuous Active Sensing (`0xFE`) or Clock (`0xF8`) bytes, or when large multi-packet SysEx dumps arrive, the kernel input pool fills up completely.
3. When the queue overflows, the kernel driver sets the client error state to `-ENOSPC` and **automatically flushes the entire input queue**.
4. The subsequent call to `snd_seq_event_input()` returns `-ENOSPC`.

#### The Four-Part Architectural Solution:

1. **Massive Pool Expansion:**
   Immediately upon opening the sequencer client, expand the kernel input pool from the default 200 cells to **32,768 cells**, and the output buffer to **128 KB**:
   ```cpp
   snd_seq_set_client_pool_input(seq_, 32768);
   snd_seq_set_output_buffer_size(seq_, 131072);
   ```

2. **Non-Blocking Asynchronous Mode:**
   Switch the ALSA handle to non-blocking mode (`snd_seq_nonblock(seq_, 1)`). This ensures the application never blocks indefinitely inside kernel space.

3. **Graceful `-ENOSPC` and `-EAGAIN` Recovery:**
   In `Device::receive_raw()`, treat `-ENOSPC` as a recoverable transient condition rather than a fatal error:
   ```cpp
   snd_seq_event_t* ev = nullptr;
   int err = snd_seq_event_input(seq_, &ev);
   if (err == -ENOSPC) {
       // Kernel input buffer overflowed and was cleared automatically.
       // Do not throw; continue polling for the remaining data.
       std::this_thread::sleep_for(std::chrono::milliseconds(1));
       continue;
   }
   if (err == -EAGAIN) {
       // No event waiting; yield and poll
       std::this_thread::sleep_for(std::chrono::milliseconds(1));
       continue;
   }
   ```

4. **Input Draining & UI Debouncing:**
   - Implement `clear_input()` before issuing new commands to discard accumulated Active Sensing and Clock events.
   - Introduce a 200 ms debounce timer on table selection changes in `MainWindow` to prevent rapid keyboard navigation from flooding the device.

---

### 10.2 The Silent Audio Playback Failure in Modern Linux Desktop Environments

#### Symptom:
Users could not preview samples within the GUI. Clicking "Play Preview" produced silence, and the console reported:
```
QAudioSink: open error - no audio device found
```

#### Root-Cause Analysis:
The initial GUI implementation relied on `QAudioSink` from the **Qt 6 Multimedia** module. Under Linux, Qt Multimedia relies on a GStreamer backend plugin. On many modern Linux distributions running PipeWire:
1. `QMediaDevices::defaultAudioOutput().isNull()` returned `true` due to missing GStreamer plugin packages (`gstreamer1.0-pipewire` or `gstreamer1.0-alsa`).
2. Even with the packages installed, Qt Multimedia frequently failed to negotiate 31,250 Hz single-channel mono streams with the audio daemon, throwing `QAudio::OpenError`.

#### The Solution: Zero-Dependency Embedded ALSA PCM Engine
Qt Multimedia was removed entirely for sample playback and replaced with **`AlsaAudioPlayer`** (`audio_player.hpp`, `audio_player.cpp`), a native ALSA PCM audio player:
- Communicates directly with ALSA via `snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0)`.
- Eliminates dependencies on GStreamer, PulseAudio client libraries, and desktop multimedia plugins.
- Implements automatic mono-to-stereo sample replication fallback for DACs that do not natively support single-channel streams.

---

### 10.3 Embedded USB Microcontroller FIFO Overruns

#### Symptom:
Uploading samples larger than 30 kB frequently failed, with the Volca locking up and requiring a power cycle.

#### Root-Cause Analysis:
The Volca Sample 2's USB receiver has a limited hardware FIFO buffer. Sending large payloads at full USB speed causes the microcontroller's FIFO to overflow, dropping bytes and corrupting the SysEx packet.

#### The Solution: 256-Byte Chunk Slicing with Throttling
All outgoing SysEx streams are partitioned into blocks of at most **256 bytes**, with a **10 ms inter-chunk cooldown delay** (`std::this_thread::sleep_for(std::chrono::milliseconds(10))`). This provides the embedded microcontroller sufficient time to process each incoming block and write it to flash memory.

---

### 10.4 The `SampleFull` Sector Exhaustion Failure During Package Uploads

#### Symptom:
When uploading an official or custom preset package (`.ivlcsplpreset`) containing 24 samples to a Volca Sample 2 that already possessed 90 previously loaded samples, the upload failed abruptly at sample 18 with:
```
volsa2::NakError: Received NAK (SampleFull, 0x25)
```
Inspection showed that the 24 new samples together required only 3,200 sectors out of the total 8,192 flash sectors, well within total hardware capacity.

#### Root-Cause Analysis:
The Volca Sample 2 flash controller allocates contiguous sectors on a first-come, first-served basis without garbage collection across unrelated slots:
1. If the hardware holds 90 occupied samples (e.g. slots 0–89 consuming 6,500 sectors), only 1,692 free sectors remain in the pool.
2. In a naive upload loop, the host begins streaming samples from the package (slot 0, slot 1, ...).
3. The hardware attempts to write each incoming sample. However, the existing large samples in slots 24–89 from the *previous* session are still holding their assigned flash sectors because they have not been overwritten or erased.
4. Once the cumulative memory of newly written samples exceeds the remaining 1,692 sectors, the hardware returns `NakStatus::SampleFull` (`0x25`).

#### The Solution: Two-Phase Pre-Clean Reclaim Protocol:
Before transmitting any new audio PCM data, VolSa 2 initiates **Phase 1: Pre-Clean Sector Reclaim**:
1. It queries on-board headers for slots 0–199 to identify slots occupied on hardware.
2. It compares them against the package: any slot occupied on the device that is *not* present in the new package is marked for reclamation.
3. It systematically wipes these obsolete slots by transmitting `SampleHeader::empty(slot)`.
4. As the device receives each empty header, it frees the associated flash sectors back into the memory pool.
5. In **Phase 2**, the package's active samples are uploaded into the reclaimed, maximized free memory space with 100% deterministic success.

---

### 10.5 The Qt `slots` Preprocessor Macro Keyword Collision Trap

#### Symptom:
During the implementation of multi-sample batch selection and pattern operations, the C++ compiler threw cryptic syntax errors inside GUI headers and source files:
```
error: expected unqualified-id before 'slots'
error: expected primary-expression before 'slots'
error: cannot declare 'slots' to be a member of 'DeviceWorker'
```

#### Root-Cause Analysis:
In Qt's core header `<qobjectdefs.h>`, the keywords `signals` and `slots` are defined as C preprocessor macros:
```cpp
#define slots Q_SLOTS
#define signals Q_SIGNALS
```
When compiling without `-DQT_NO_KEYWORDS`, the preprocessor blindly replaces every occurrence of the token `slots` in C++ source files:
```cpp
// Intended C++20 code:
void eraseSamples(const std::vector<uint8_t>& slots);

// Expanded by preprocessor into:
void eraseSamples(const std::vector<uint8_t>& Q_SLOTS); // SYNTAX ERROR!
```

#### The Engineering Invariant:
1. **Naming Discipline:** In any project integrating Qt, **never** name a local variable, function parameter, member variable, or type `slots`.
2. **Disambiguated Identifiers:** Always use `slot_indices`, `sel`, `slot_list`, or `active_slots`:
   ```cpp
   void eraseSamples(const std::vector<uint8_t>& slot_indices);
   std::vector<uint8_t> selectedSlots() const;
   ```
3. **Macro Isolation:** In CMake target definitions, standardizing on `-DQT_NO_KEYWORDS` enforces the use of explicit `Q_SLOTS` and `Q_SIGNALS`, preventing third-party variable names from conflicting with Qt macros.

---

## 11. Command-Line Interface (`volsa2-cli`) Reference Manual

`volsa2-cli` offers a deterministic, scriptable command-line interface suitable for automated batch processing, headless Linux installations, and continuous integration environments.

### 11.1 CLI Design Philosophy & Execution Mechanics
- **Zero Daemon Dependencies:** Connects directly to the Linux ALSA subsystem without requiring PipeWire, PulseAudio, or Jack daemons.
- **Fail-Fast Error Diagnostics:** Translates hardware NAK codes into clear terminal error descriptions and non-zero exit codes.
- **Progress Telemetry:** Features real-time terminal progress reporting for high-volume sample and package transfers.

### 11.2 Subcommand Reference: `list`, `download`, `upload`, `remove`

#### 1. List (`list`, `ls`)
Prints all sample slots loaded into device memory and displays overall sector occupancy:
```sh
volsa2-cli list [-a | --show-empty]
```

**Example Output:**
```
Device connected: volca sample (Firmware v1.02, Channel 1)
Memory: 5,420 / 8,192 sectors used (66.16%)

Slot  Name                  Length (Samples)  Duration  Speed  Level
---------------------------------------------------------------------
000   KICK_01                          12450     0.40s  16384  65535
001   SNARE_909                        18200     0.58s  16384  65535
002   HIHAT_CL                          6800     0.22s  16384  65535
...
```

#### 2. Download (`download`, `dl`)
Downloads audio from a slot and serializes it as a 16-bit 31.25 kHz RIFF WAV file:
```sh
volsa2-cli download <slot-index> [-o | --output <path>]
```
- If `--output` is omitted, the file is automatically named `<slot-index>_<name>.wav`.

#### 3. Upload (`upload`, `up`)
Converts an audio file to 31.25 kHz mono linear PCM and uploads it to hardware:
```sh
volsa2-cli upload <file> [slot] [-m | --mono-mode <mid|left|right|side>] [-o | --output <path>] [--dry-run]
```
- If `[slot]` is omitted, the first vacant slot is located and populated automatically.
- If the destination slot is occupied, an interactive confirmation prompt is presented:
  ```
  Slot 42 is already occupied by "OLD_SNARE" (14,200 samples).
  Overwrite? [y/N]: 
  ```
- `--dry-run`: Performs file inspection, conversion, and optional WAV export without sending MIDI data to hardware.

#### 4. Remove (`remove`, `rm`)
Erases a sample from device memory:
```sh
volsa2-cli remove <slot-index> [-p | --print-name]
```

---

### 11.3 Package Subcommands: `download-package` (`pkg-dl`), `upload-package` (`pkg-up`)

#### 1. Download Package (`download-package`, `pkg-dl`)
Downloads all 16 sequencer patterns and up to 200 samples from the Volca Sample 2 and bundles them into an official KORG `.ivlcsplpreset` ZIP archive:
```sh
volsa2-cli download-package <output-file.ivlcsplpreset> [options]
```

##### Options:
- `--samples-only`: Restricts the download to sample metadata and PCM waveforms, omitting patterns.
- `--patterns-only`: Restricts the download to the 16 sequencer patterns, omitting samples.

**Example:**
```sh
volsa2-cli download-package full_backup_2026.ivlcsplpreset
```

#### 2. Upload Package (`upload-package`, `pkg-up`)
Restores an `.ivlcsplpreset` preset archive onto the device:
```sh
volsa2-cli upload-package <input-file.ivlcsplpreset> [options]
```

##### Options:
- `--samples-only`: Restores sample slots only.
- `--patterns-only`: Restores sequencer pattern programs only.
- `--clear-empty` / `--erase-empty` (Default): Activates the **Pre-Clean Reclaim Protocol** (Phase 1) to erase obsolete hardware slots and eliminate `SampleFull` errors.
- `--keep-existing`: Disables Phase 1 pre-cleaning, preserving unrelated existing samples on the hardware.
- `-y, --yes`: Suppresses interactive confirmation prompts.
- `--dry-run`: Inspects package contents and displays planned operations without modifying device state.

**Example:**
```sh
volsa2-cli upload-package techno_kit.ivlcsplpreset --clear-empty -y
```

---

### 11.4 Automation & Shell Scripting Integration

#### Example 1: Full Automated Machine Backup
```bash
#!/usr/bin/env bash
set -euo pipefail
BACKUP_DIR="./volca_backups/$(date +%Y%m%d_%H%M%S)"
mkdir -p "$BACKUP_DIR"

echo "Archiving complete device state to KORG preset package..."
volsa2-cli download-package "$BACKUP_DIR/full_state.ivlcsplpreset"

echo "Exporting individual WAV stems..."
mkdir -p "$BACKUP_DIR/stems"
for slot in $(seq 0 199); do
    volsa2-cli download "$slot" -o "$BACKUP_DIR/stems/slot_${slot}.wav" 2>/dev/null || true
done
echo "Backup successfully completed at $BACKUP_DIR"
```

#### Example 2: Batch Processing and Loading an MPC Drum Kit
```bash
#!/usr/bin/env bash
set -euo pipefail

# Convert and upload a folder of 16 drum hits to slots 0..15
slot=0
for sample_file in ./mpc_drums/*.wav; do
    echo "Uploading $sample_file to slot $slot (Mid downmix)..."
    volsa2-cli upload "$sample_file" "$slot" --mono-mode mid
    ((slot++))
    if [ "$slot" -ge 16 ]; then break; fi
done
echo "MPC Kit loaded successfully into slots 0-15."
```

---

## 12. Quality Assurance, Test Suites & Verification

VolSa 2 maintains an automated test suite executed via CTest, covering bit-level packing math, hardware SysEx golden captures, DSP filters, pattern sequences, package archival, and transient chopping algorithms.

### 12.1 Property-Based 7-Bit Round-Trip Testing (`test_seven_bit`)
`test_seven_bit.cpp` validates the 7-bit packing and unpacking algorithms across buffer sizes from 0 to 65,536 bytes:
- Validates that `U7::take_nth_msb(n)` extracts the correct bit.
- Verifies that $L_{u8 \to u7}$ matches `U8ToU7::convert_len()`.
- Verifies that $L_{u7 \to u8}$ matches `U7ToU8::convert_len()`.
- Validates the round-trip invariant:
  $$\text{U7ToU8}(\text{U8ToU7}(X)) = X \quad \forall X \in \text{ByteBuffers}$$
- Enforces the $8n + 1$ invalidation law, ensuring corrupt streams trigger `ProtocolError`.

### 12.2 Golden-File Hardware Capture Validation (`test_proto`)
`test_proto.cpp` validates the C++ parser and serializer against actual raw SysEx hardware dumps captured from a physical Volca Sample 2 (`test_data/sample_data_dump1.raw` through `sample_data_dump14.raw`):
1. Loads each raw hardware dump.
2. Parses the packet with `volsa2::SampleData::parse()`.
3. Verifies that the reconstructed 16-bit PCM samples match the reference audio file (`sampleX.wav.raw`).
4. Re-encodes the sample data via `sample_data.encode(0)` and verifies that the serialized output matches the original hardware capture byte-for-byte.

### 12.3 DSP Precision & Mathematical Resampling Tests (`test_audio`)
`test_audio.cpp` tests the audio processing pipeline:
- Validates that Mid, Left, Right, and Side downmixing produce mathematically exact results.
- Verifies that resampling a 44.1 kHz sine wave to 31.25 kHz maintains frequency and phase fidelity without aliasing artifacts.
- Verifies that `write_wav_file` produces a valid 44-byte RIFF WAVE container readable by `libsndfile`.

### 12.4 Mock ALSA Loopback Verification
Validates client and port connection topology using kernel dummy sequencer devices (`snd-seq-dummy`), verifying bidirectional non-blocking event flow and cooldown pacing without physical hardware attached.

### 12.5 Sequencer Pattern SysEx Verification (`test_pattern_proto`)
`test_pattern_proto.cpp` validates the sequencer pattern communication layer:
- **Request Framing:** Validates serialization of `PatternDumpRequest` (0x1D) across all 16 slots ($p \in [0, 15]$).
- **Pattern Payload Round-Trip:** Creates synthetic pattern buffers containing the `'PTST'` header, populates 7,936 bytes with test sequences, encodes them to 9,079-byte SysEx messages (0x4D), and verifies exact round-trip byte identity via `PatternData::parse()`.
- **In-Place Name Mutation:** Mutates the 48-byte ASCII name at offset 16 and asserts that all 7,872 remaining sequence bytes remain bit-identical.
- **Malformed Packet Rejection:** Verifies that truncated buffers (< 9,079 bytes), invalid headers, or missing `0xF7` terminators throw descriptive `ProtocolError` exceptions.

### 12.6 Package Archival & Round-Trip Deserialization Tests (`test_package`)
`test_package.cpp` validates the `.ivlcsplpreset` PKZIP engine:
- **MD5 Hash Verification:** Validates that `compute_md5_hex()` produces standard RFC 1321 cryptographic digests matching known test vectors.
- **Container Generation:** Creates a full `PackageData` model comprising 16 patterns and 200 samples with PCM waveforms.
- **Serialization & Deserialization:** Calls `save_package()` to generate an `.ivlcsplpreset` archive, asserts non-zero file creation, and parses it back using `load_package()`.
- **Integrity Validation:** Verifies that `FileInformation.xml`, `PresetInformation.xml`, `Prog_*.prog_info`, `Prog_*.prog_bin`, `Smpl_*.smpl_info`, and `Smpl_*.smpl_bin` are extracted without data loss or checksum divergence.

### 12.7 Transient Chopper Detection & Slicing Tests (`test_sample_chopper`)
`test_sample_chopper.cpp` verifies the audio slicing algorithms:
- **Audio Cropping & Micro-Fades:** Verifies sub-span sample extraction and checks that head/tail samples are smoothly attenuated by the 32-sample half-cosine window.
- **Silence Trimming:** Generates audio buffers containing leading and trailing silence and verifies that `find_silence_bounds()` and `auto_trim_silence()` isolate the active acoustic payload within $\pm 64$ samples.
- **Equal-Division Grid:** Verifies that `divide_equal_slices()` partitions arbitrary buffer sizes into $N$ contiguous slices without gaps or overlaps.
- **Transient Onset Detection:** Verifies that synthetic impulse trains (drum hits separated by silence) are correctly located by `detect_transient_slices()` and respect the minimum refractory distance.

```sh
$ ctest --test-dir build --output-on-failure
```

---

## 13. Build System, Deployment & Troubleshooting

### 13.1 Dependency Matrix & System Prerequisites

| Package Name | Debian/Ubuntu Package | Fedora Package | Arch Linux Package | Purpose |
|---|---|---|---|---|
| **C++20 Compiler** | `g++` (>= 11) or `clang` | `gcc-c++` | `gcc` | Language features (`std::span`, `<bit>`) |
| **Build Tools** | `cmake`, `pkg-config` | `cmake`, `pkgconf` | `cmake`, `pkgconf` | Build configuration & dependency detection |
| **ALSA Library** | `libasound2-dev` | `alsa-lib-devel` | `alsa-lib` | ALSA Sequencer & PCM audio subsystems |
| **Audio Resampler** | `libsamplerate0-dev` | `libsamplerate-devel` | `libsamplerate` | Band-limited sinc polyphase resampling |
| **Audio Codec Library** | `libsndfile1-dev` | `libsndfile-devel` | `libsndfile` | Multi-format audio file decoding & WAV I/O |
| **ZIP Archive Library** | `libzip-dev` | `libzip-devel` | `libzip` | `.ivlcsplpreset` ZIP container support |
| **OpenSSL Crypto** | `libssl-dev` | `openssl-devel` | `openssl` | MD5 checksum computation (`EVP_DigestInit_ex`) |
| **Qt 6 Framework** | `qt6-base-dev` | `qt6-qtbase-devel` | `qt6-base` | Core, GUI, and Widgets modules |

#### Installation on Debian/Ubuntu:
```sh
sudo apt update
sudo apt install build-essential cmake pkg-config \
    libasound2-dev libsamplerate0-dev libsndfile1-dev \
    libzip-dev libssl-dev qt6-base-dev
```

---

### 13.2 CMake Build Configurations & Optimization Flags

#### Full Build (CLI, GUI, and Tests):
```sh
cd volsa2-cpp
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

#### Headless Build (Core Library and CLI Only):
For embedded systems, servers, or headless machines without Qt:
```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_GUI=OFF
cmake --build build -j$(nproc)
```

---

### 13.3 Linux udev Permissions & Real-Time Audio Configuration

#### 1. Audio Group Membership:
Access to ALSA device nodes (`/dev/snd/seq`, `/dev/snd/pcm*`) requires membership in the `audio` group:
```sh
sudo usermod -aG audio $USER
```
Log out and log back in for changes to take effect.

#### 2. Custom udev Rule:
To guarantee consistent device permissions regardless of desktop environment, create `/etc/udev/rules.d/99-korg-volca.rules`:
```udev
# KORG Volca Sample 2 USB MIDI Interface
SUBSYSTEM=="sound", ATTRS{idVendor}=="0944", MODE="0666", GROUP="audio"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0944", MODE="0666", GROUP="audio"
```
Reload udev rules:
```sh
sudo udevadm control --reload-rules && sudo udevadm trigger
```

---

### 13.4 Diagnostic Toolkit

If the hardware is not recognized or errors occur, use these standard Linux diagnostic utilities:

#### 1. Check USB Device Enumeration:
```sh
lsusb -d 0944:
```
Expected output:
```
Bus 001 Device 014: ID 0944:013b KORG, Inc. volca sample
```

#### 2. Check ALSA Sequencer Client Registration:
```sh
aconnect -l
```
Expected output:
```
client 24: 'volca sample' [type=kernel,card=2]
    0 'volca sample MIDI 1'
```

#### 3. Test Direct SysEx Query via `amidi`:
```sh
amidi -p hw:2,0,0 -S "F0 42 50 00 2A F7" -d
```
Expected response:
```
F0 42 50 01 00 2A 2D 01 08 00 ... F7
```

---

## 14. Summary & Conclusion

**VolSa 2** provides a complete, modern C++20 and Qt 6 sample librarian and sequencer management workstation for the KORG Volca Sample 2:
- Translates the original Rust architecture into clean, idiomatic modern C++20 with strong type safety and zero-overhead memory abstractions.
- Implements the complete KORG SysEx protocol including sample transfers, on-board sequencer pattern dumps (0x4D), and non-destructive pattern renaming.
- Supports official KORG `.ivlcsplpreset` package archives with a two-phase pre-clean sector reclaim algorithm that eliminates flash sector exhaustion and `SampleFull` aborts.
- Embeds a high-performance audio DSP engine for 31,250 Hz sinc resampling, silence auto-trimming, anti-click micro-fades, and short-time energy transient slicing.
- Features a responsive, multi-threaded Qt 6 desktop application equipped with multi-sample extended selection, batch operations, drag-and-drop ingestion, interactive waveforms, and a sequencer pattern manager.
- Communicates directly with the Linux kernel via the ALSA Sequencer and ALSA PCM APIs, delivering zero-dependency, low-latency audio playback and rock-solid hardware communication.
