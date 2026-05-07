# MAAPS Nexus Plugins Repository

This repository contains the CLAP and ARA 2.0 plugins/extensions for the MAAPS Nexus multi-agent audio processing system.

## Contents

### 1. CLAP Plugin (`clap-plugin/`)
- **Mamba Compressor**: A lookahead feedforward compressor with soft-knee gain computer and IIR ballistics
- **Format**: CLAP (C/LV2 Audio Plugin) format for universal DAW compatibility
- **Features**:
  - 8 adjustable parameters (Threshold, Ratio, Knee, Attack, Release, Makeup Gain, Lookahead, Mix)
  - Lookahead processing for transient preservation
  - Soft-knee compression for musical smoothing
  - IIR ballistics for natural attack/release characteristics
  - Dry/wet mix for parallel compression
  - <10ms latency target

### 2. ARA 2.0 Extension (`ara-extension/`)
- **ARA Communicator**: Bridge for ARA 2.0 host extensions enabling offline audio analysis
- **Features**:
  - Spectral analysis (FFT, spectral centroid, rolloff, MFCCs, flux)
  - Content analysis (key, tempo, chords, instruments, genre)
  - Melody and chord extraction (MIDI-compatible output)
  - Processing suggestions (EQ and Dynamics from trained RL agents)
  - Separation potential analysis (vocal/drums/bass/other)
  - Groove analysis (swing, timing consistency, beat strength)
  - Dynamics analysis (loudness, transient consistency, compression suggestions)

## Building and Installation

### CLAP Plugin
```bash
cd clap-plugin
./build.sh
# Installs to ~/.clap/MambaCompressor.clap (Linux)
# Rescan plugins in your DAW to detect the new plugin
```

### ARA Extension
The ARA extension is designed to be integrated into DAW ARA 2.0 host plugins.
It provides a Python-based analysis backend that communicates via IPC.

## Dependencies

### CLAP Plugin
- C++ compiler with C++17 support
- CMake 3.21+
- CLAP framework (automatically fetched via CMake)

### ARA Extension
- Python 3.7+
- Required packages: numpy, librosa, soundfile
- Optional (for full MAAPS integration): ray, torch

## Usage

### CLAP Plugin in DAW
1. After building, the plugin will be available in your DAW's plugin list
2. Insert on audio tracks for professional compression
3. Adjust parameters to taste or use suggested settings from ARA analysis

### ARA Extension in DAW
1. Requires an ARA 2.0 host plugin that communicates with this Python backend
2. When analyzing audio regions in your DAW, the ARA host will request analysis
3. Results are returned to the DAW for display and further processing

## Development

Both plugins are designed to integrate with the MAAPS Nexus multi-agent audio processing system:
- CLAP plugin provides real-time processing
- ARA extension provides offline analysis and AI-powered suggestions

For more information about MAAPS Nexus, see the main repository documentation.