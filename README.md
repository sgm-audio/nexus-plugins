# Mamba Compressor (nexus-plugins)

A lookahead feedforward compressor in three separate pieces that share the same DSP: a CLAP plugin, a JUCE VST3/Standalone build with a GUI and ARA scaffolding, and a Python offline analysis backend. Despite the name, there's no Mamba/state-space model in here. It's a conventional compressor.

What's been checked: the Python backend's 16 tests pass (Python 3.12, librosa, Linux x86_64). The CLAP and JUCE builds weren't built or loaded in a host in my last pass, because that box had no C++ compiler.

## Components

### 1. CLAP Plugin — `clap-plugin/`
- Raw CLAP C API, single-file DSP (`src/mamba_compressor.cpp`)
- 8 parameters: Threshold, Ratio, Knee, Attack, Release, Makeup Gain, Lookahead, Mix
- Lookahead feedforward topology · soft-knee gain computer · IIR ballistics · dry/wet parallel
- Lookahead is 0-50 ms (default 5 ms) and is reported to the host as latency. That's a design limit, not a measurement. State save/load is in.
- No GUI (`CLAP_EXT_GUI` not implemented)

```bash
cd clap-plugin && ./build.sh   # installs MambaCompressor.clap
```

### 2. JUCE Frontend — `juce-frontend/`
- JUCE 7+ VST3 + Standalone. The CMake config turns on ARA (`IS_ARA_EFFECT`) and there's a document controller in `Source/MambaARADocumentController.h`, but it hasn't been tested in an ARA host.
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

- No CI, no presets, no AU/AAX
- CLAP and JUCE builds not verified recently (see above)
- No latency or CPU measurements yet
- State save/load has no version header
- `get_metadata` uses `librosa.beat.beat_track` (slow on long files, no caching)

## License

MIT. See [LICENSE](LICENSE). JUCE and the ARA SDK aren't included and come under their own licences.
