# MAAPS Nexus Plugins — TODO

## Completed

- [x] **CLAP install permission fix** — set `CLAP_INSTALL OFF` and `EXCLUDE_FROM_ALL` on FetchContent; `cmake --install` now succeeds without writing to `/usr/local`
- [x] **ARA Agent Dispatch** — `_handle_analyze_region` now routes to all 8 Analyzer Agents with real analysis; returns per-agent results and errors
- [x] **Replace mock ARA data with real analysis** — all 8 agents (Spectral, Content, Melody, Chord, Separation, Groove, Dynamics, ProcessingSuggestions) now use librosa on real audio data
- [x] **ARA `_handle_analyze_spectral` was using random noise** — now loads actual audio via `AudioLoader`
- [x] **Fix GrooveAnalyzer `swing` variable scope bug** — `swing` was only defined in `if` branch but referenced unconditionally
- [x] **Add `.gitignore`** — project root now has a gitignore covering build/, __pycache__/, .pytest_cache/, CMake artefacts, plugin binaries
- [x] **Add `__init__.py`** — `ara-extension/src/` and `ara-extension/src/daw_integration/` now have proper package files
- [x] **Add `requirements.txt`** — `ara-extension/requirements.txt` with numpy, librosa, soundfile, pyzmq
- [x] **Fix pytest test structure** — tests refactored from `if __name__ == "__main__"` pattern into proper `test_` functions with assertions; no return values (was causing `PytestReturnNotNoneWarning`)
- [x] **Add `conftest.py`** — shared fixtures for audio generation (WAV), communicator setup/teardown
- [x] **Tests use real audio** — fixtures generate WAV files with sine signals; no more mock `audio_source_id: "track_123"` that bypasses loading
- [x] **Update README** — added JUCE frontend section, build instructions for all 3 components, updated ARA description to reflect real analysis agents

## Remaining

- [ ] **JUCE frontend only builds VST3 + Standalone** — AU and AAX formats not configured (macOS/Windows)
- [ ] **No `CLAP_EXT_GUI` support** in CLAP plugin — no custom GUI, relies on host generic UI
- [ ] **No CI/CD pipeline** — no GitHub Actions for automated build + test
- [ ] **State save/load has no version header** — both CLAP and JUCE state formats are raw doubles / XML without a version tag for forward compatibility
- [ ] **`get_metadata` uses librosa.beat.beat_track** — can be slow on very long files; consider caching or async
- [ ] **ARA IPC still mock** — `_ipc_loop` reads from a local queue; production needs real ZeroMQ socket wiring
- [ ] **No preset system** — no factory presets or preset browser in any format
