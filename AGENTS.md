# AGENTS.md — Nexus-Plugins

3 independent components with the same compressor DSP, built separately. No top-level build system.

## Layout

- `clap-plugin/` — CLAP plugin (single `src/mamba_compressor.cpp`, CMake + FetchContent for CLAP 1.2.2)
- `juce-frontend/` — JUCE VST3 + Standalone with GUI and ARA support (no AU/AAX)
- `ara-extension/` — Python offline audio analysis backend (8 agents via librosa)

## Build

### CLAP Plugin

```bash
cd clap-plugin && ./build.sh
```

- `build.sh` runs `cmake -B build -DCMAKE_BUILD_TYPE=Release` → `cmake --build` → `cmake --install`
- Installs to `~/.clap/MambaCompressor.clap` (Linux), `~/Library/Audio/Plug-Ins/CLAP/` (macOS)
- CLAP fetched via FetchContent with `CLAP_INSTALL OFF` + `EXCLUDE_FROM_ALL` — **do not revert**, prevents system-wide install
- Flags: `-O3 -march=native -ffast-math`; C++17; CMake 3.21+

### JUCE Frontend

```bash
cd juce-frontend && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release
```

- Requires `$HOME/JUCE` (JUCE 7+). Override: `-DJUCE_PATH=/path/to/JUCE`
- Requires `$HOME/ARA_SDK` (ARA SDK 2.2.0). Override: `-DARA_SDK_PATH=...`
- `COPY_PLUGIN_AFTER_BUILD=ON` — VST3 auto-installs to system plugin dir
- Standalone binary: `build/MambaCompressor_artefacts/Standalone/`
- CMake 3.22+; C++17; `-O2` (not `-O3` like CLAP)

### ARA Extension

```bash
cd ara-extension && pip install -r requirements.txt && pytest -v
```

- Tests generate WAV fixtures at runtime (sine signals via numpy+soundfile) — no external audio files needed
- Tests use `send_mock_request`/`get_mock_response` (local `queue.Queue`), not real ZeroMQ
- Slow tests (e.g. `test_analyze_region_with_audio`) use `timeout=15.0` due to librosa processing
- 16 tests total (3 in `test_ara_comm.py`, 13 in `test_ara_extensions.py`)
- `ray` and `torch` mentioned in README as optional but **not imported anywhere** — do not install them

## Architecture

- DSP is implemented **twice**: `clap-plugin/src/mamba_compressor.cpp` (raw CLAP C API, 543 lines) and `juce-frontend/Source/MambaCompressorDSP.h` (standalone `CompressorDSP` class, 110 lines). Changes to one do **not** affect the other — keep them in sync manually.
- JUCE processor extends `AudioProcessorARAExtension` — it is both a VST3 and an ARA effect
- JUCE GUI (`PluginEditor.cpp/h`) reads `gainReductionDb` atomic from the processor for the GR meter
- ARA: `ara_wrapper.py` is a single 687-line module with `AudioLoader`, 8 `AnalyzerAgent` subclasses, and `AraPluginCommunicator`
- `AraPluginCommunicator._ipc_loop` uses a local `queue.Queue` — production ZeroMQ wiring not yet implemented

## Known Gaps

- No `CLAP_EXT_GUI` — CLAP plugin has no custom GUI
- State save/load has no version header (raw doubles in CLAP, XML in JUCE)
- `get_metadata` uses `librosa.beat.beat_track` — slow on long files, no caching
- No CI/CD, no preset system, no AU/AAX formats
