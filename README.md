# MAAPS Nexus Plugins Repository

This repository contains the CLAP, JUCE/VST3, and ARA 2.0 plugins/extensions for the MAAPS Nexus multi-agent audio processing system.

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
  - State save/load for presets
  - Latency reporting to host

### 2. JUCE Frontend (`juce-frontend/`)
- **Mamba Compressor**: Same DSP engine wrapped in a JUCE plugin with a custom GUI
- **Formats**: VST3, Standalone
- **GUI**: Dark theme with 8 rotary knobs, purple accent, and a gain reduction meter
- **Features**: Same 8 parameters as the CLAP version, with real-time GR meter visualization

### 3. ARA 2.0 Extension (`ara-extension/`)
- **ARA Communicator**: Bridge for ARA 2.0 host extensions enabling offline audio analysis
- **Analysis Agents** (real librosa-based analysis, no mock data):
  - **Spectral**: FFT, spectral centroid, rolloff, MFCCs, flux
  - **Content**: Key estimation (Krumhansl-Schmuckler), tempo, energy
  - **Melody Extraction**: pYin pitch tracking with MIDI-compatible note output
  - **Chord Extraction**: Chroma-based chord template matching
  - **Processing Suggestions**: Data-driven EQ and dynamics recommendations
  - **Separation Potential**: Spectral feature-based stem separation scoring
  - **Groove**: Swing, timing consistency, tempo stability, beat strength
  - **Dynamics**: Loudness (LUFS), dynamic range, crest factor, compression suggestions

## Building and Installation

### CLAP Plugin
```bash
cd clap-plugin
./build.sh
# Installs to ~/.clap/MambaCompressor.clap (Linux)
# ~/Library/Audio/Plug-Ins/CLAP/ (macOS)
# %COMMONPROGRAMFILES%/CLAP/ (Windows)
# Rescan plugins in your DAW to detect the new plugin
```

### JUCE Frontend (VST3 + Standalone)
```bash
cd juce-frontend
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
# VST3 installs to system plugin dir automatically (COPY_PLUGIN_AFTER_BUILD=ON)
# Standalone binary in build/MambaCompressor_artefacts/Standalone/
```
Requires a local JUCE installation at `$HOME/JUCE`. Override with `-DJUCE_PATH=/path/to/JUCE`.

### ARA Extension
The ARA extension is designed to be integrated into DAW ARA 2.0 host plugins.
It provides a Python-based analysis backend that communicates via IPC.

```bash
cd ara-extension
pip install -r requirements.txt
pytest -v   # run test suite (16 tests)
```

## Dependencies

### CLAP Plugin
- C++ compiler with C++17 support
- CMake 3.21+
- CLAP framework (automatically fetched via CMake FetchContent)

### JUCE Frontend
- C++ compiler with C++17 support
- CMake 3.22+
- JUCE 7+ (local install at `$HOME/JUCE`)

### ARA Extension
- Python 3.7+
- Required packages: see `ara-extension/requirements.txt`
  - numpy, librosa, soundfile, pyzmq
- Optional (for full MAAPS integration): ray, torch

## Usage

### CLAP Plugin in DAW
1. After building, the plugin will be available in your DAW's plugin list
2. Insert on audio tracks for professional compression
3. Adjust parameters to taste or use suggested settings from ARA analysis

### JUCE Frontend in DAW
1. Build the VST3; it auto-installs to the system plugin directory
2. Scan for new plugins in your DAW
3. The GUI provides 8 rotary knobs and a real-time gain reduction meter

### ARA Extension in DAW
1. Requires an ARA 2.0 host plugin that communicates with this Python backend
2. When analyzing audio regions in your DAW, the ARA host will request analysis
3. Results are returned to the DAW for display and further processing
4. The `analyze_region` endpoint dispatches to all 8 analysis agents in parallel
5. Individual agent endpoints (e.g. `analyze_spectral`, `analyze_dynamics`) are also available

## Development

Both plugins are designed to integrate with the MAAPS Nexus multi-agent audio processing system:
- CLAP plugin provides real-time processing
- JUCE frontend provides VST3 + Standalone with GUI
- ARA extension provides offline analysis and AI-powered suggestions

For more information about MAAPS Nexus, see the main repository documentation.
