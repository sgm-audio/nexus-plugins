# Mamba Compressor

A real-time **lookahead feedforward compressor** shipped as three independent components that share the same DSP: a CLAP plugin, a JUCE (VST3 / Standalone) frontend with GUI + ARA, and a Python offline analysis backend.

## Components

### 1. CLAP Plugin — `clap-plugin/`
- Raw CLAP C API, single-file DSP (`src/mamba_compressor.cpp`)
- 8 parameters: Threshold, Ratio, Knee, Attack, Release, Makeup Gain, Lookahead, Mix
- Lookahead feedforward topology · soft-knee gain computer · IIR ballistics · dry/wet parallel
- < 10 ms latency target · state save/load · host latency reporting
- No GUI (`CLAP_EXT_GUI` not implemented)

```bash
cd clap-plugin && ./build.sh   # installs MambaCompressor.clap
```

### 2. JUCE Frontend — `juce-frontend/`
- JUCE 7+ **VST3 + Standalone** with ARA 2.0 support
- Same 8-parameter DSP (`Source/MambaCompressorDSP.h`) behind a dark-theme GUI (8 rotary knobs + live gain-reduction meter)
- Requires `$HOME/JUCE` and `$HOME/ARA_SDK` (override via `-DJUCE_PATH=`, `-DARA_SDK_PATH=`)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release
```

### 3. ARA Extension — `ara-extension/`
- Python offline analysis backend — 8 librosa-based analysis agents (spectral, content, melody, chord, processing suggestions, separation potential, groove, dynamics)
- Single module `src/daw_integration/ara_wrapper.py`; IPC via in-process queue (ZeroMQ not yet wired)

```bash
pip install -r requirements.txt && pytest -v   # 16 tests, fixtures generated at runtime
```

## Architecture notes

- DSP is implemented **twice** — `clap-plugin/src/mamba_compressor.cpp` and `juce-frontend/Source/MambaCompressorDSP.h`. Keep them in sync manually.
- No top-level build system; each component builds independently.

## Known gaps

- No CI/CD · no preset system · no AU/AAX formats
- State save/load has no version header
- `get_metadata` uses `librosa.beat.beat_track` (slow on long files, no caching)
