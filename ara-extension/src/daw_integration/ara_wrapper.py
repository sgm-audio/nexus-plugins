"""
ARA (Audio Random Access) Wrapper for MAAPS - DAW Integration

This module provides the bridge for ARA 2.0 integration, allowing the Python system
to access and analyze full audio clips in a random-access manner, rather than
streaming real-time buffers.

Communication flow:
1. DAW (via ARA) sends "Analyze Region" request with audio source details
2. Python reads audio file directly (memory mapped) or via shared memory blob
3. Agents perform offline analysis (spectral, temporal, structural)
4. Python sends analysis results (metadata, graphs, events) back to DAW

Protocol: Request/Response pattern via ZeroMQ or Named Pipes.
"""

import logging
import json
import threading
import queue
import time
import numpy as np
from typing import Dict, Any, Optional
import librosa
import soundfile as sf

logger = logging.getLogger(__name__)


class AudioLoader:
    """Loads and caches audio data from file paths or shared memory references."""

    def __init__(self):
        self._cache: Dict[str, tuple] = {}

    def load(self, audio_source_id: str, start: float = 0.0,
             duration: float = 0.0) -> Optional[tuple]:
        if audio_source_id in self._cache:
            y, sr = self._cache[audio_source_id]
        else:
            try:
                path = audio_source_id
                y, sr = librosa.load(path, sr=None, mono=True)
                self._cache[audio_source_id] = (y, sr)
            except Exception as e:
                logger.error(f"Failed to load audio source '{audio_source_id}': {e}")
                return None

        if start > 0.0 or duration > 0.0:
            start_sample = int(start * sr)
            if duration > 0.0:
                end_sample = start_sample + int(duration * sr)
                return y[start_sample:end_sample], sr
            return y[start_sample:], sr

        return y, sr

    def clear_cache(self):
        self._cache.clear()


class AnalyzerAgent:
    """Base class for analysis agents that process audio data."""

    def analyze(self, y: np.ndarray, sr: float,
                request: Dict[str, Any]) -> Dict[str, Any]:
        raise NotImplementedError


class SpectralAnalyzer(AnalyzerAgent):
    def analyze(self, y: np.ndarray, sr: float,
                request: Dict[str, Any]) -> Dict[str, Any]:
        spectral_centroids = librosa.feature.spectral_centroid(y=y, sr=sr)[0]
        spectral_rolloff = librosa.feature.spectral_rolloff(y=y, sr=sr)[0]
        mfccs = librosa.feature.mfcc(y=y, sr=sr, n_mfcc=13)
        spectral_flux = librosa.onset.onset_strength(y=y, sr=sr)

        return {
            "status": "ok",
            "audio_source_id": request.get("audio_source_id"),
            "analysis_type": "spectral",
            "data": {
                "spectral_centroid": spectral_centroids.tolist(),
                "spectral_rolloff": spectral_rolloff.tolist(),
                "mfccs": mfccs.tolist(),
                "spectral_flux": spectral_flux.tolist(),
                "time_steps": len(spectral_centroids),
                "sample_rate": sr
            }
        }


class ContentAnalyzer(AnalyzerAgent):
    def analyze(self, y: np.ndarray, sr: float,
                request: Dict[str, Any]) -> Dict[str, Any]:
        chroma = librosa.feature.chroma_cqt(y=y, sr=sr)
        rms = librosa.feature.rms(y=y)[0]
        tempo, beat_frames = librosa.beat.beat_track(y=y, sr=sr)
        if isinstance(tempo, np.ndarray):
            tempo = float(tempo[0])
        else:
            tempo = float(tempo)

        key, scale = self._estimate_key(chroma)
        energy = float(np.mean(rms))

        return {
            "status": "ok",
            "audio_source_id": request.get("audio_source_id"),
            "analysis_type": "content",
            "content": {
                "key": key,
                "scale": scale,
                "tempo_bpm": round(tempo, 1),
                "time_signature": "4/4",
                "instruments": [],
                "energy": round(energy, 3),
            }
        }

    @staticmethod
    def _estimate_key(chroma: np.ndarray) -> tuple:
        major_profile = np.array([6.35, 2.23, 3.48, 2.33, 4.38, 4.09,
                                  2.52, 5.19, 2.39, 3.66, 2.29, 2.88])
        minor_profile = np.array([6.33, 2.68, 3.52, 5.38, 2.60, 3.53,
                                  2.54, 4.75, 3.98, 2.69, 3.34, 3.17])
        note_names = ['C', 'C#', 'D', 'D#', 'E', 'F',
                      'F#', 'G', 'G#', 'A', 'A#', 'B']

        chroma_avg = np.mean(chroma, axis=1)
        chroma_norm = chroma_avg / (np.sum(chroma_avg) + 1e-9)

        best_corr = -1.0
        best_key = "C"
        best_scale = "Major"

        for i in range(12):
            rotated = np.roll(chroma_norm, -i)
            corr_major = np.corrcoef(rotated, major_profile)[0, 1]
            corr_minor = np.corrcoef(rotated, minor_profile)[0, 1]

            if corr_major > best_corr:
                best_corr = corr_major
                best_key = note_names[i]
                best_scale = "Major"
            if corr_minor > best_corr:
                best_corr = corr_minor
                best_key = note_names[i]
                best_scale = "minor"

        return f"{best_key} {best_scale}", best_scale


class MelodyExtractor(AnalyzerAgent):
    def analyze(self, y: np.ndarray, sr: float,
                request: Dict[str, Any]) -> Dict[str, Any]:
        f0, voiced, _ = librosa.pyin(y, fmin=librosa.note_to_hz('C2'),
                                     fmax=librosa.note_to_hz('C7'), sr=sr)
        times = librosa.times_like(f0, sr=sr)

        melody_notes = []
        prev_midi = None
        note_start = 0.0

        for i, (freq, t, v) in enumerate(zip(f0, times, voiced)):
            if v and not np.isnan(freq):
                midi = float(librosa.hz_to_midi(freq))
                midi_round = round(midi)
                if midi_round != prev_midi:
                    if prev_midi is not None:
                        melody_notes.append({
                            "midi_note": prev_midi,
                            "start": round(note_start, 3),
                            "end": round(float(t), 3),
                            "velocity": 80,
                            "confidence": 0.85
                        })
                    note_start = float(t)
                    prev_midi = midi_round
            elif prev_midi is not None:
                melody_notes.append({
                    "midi_note": prev_midi,
                    "start": round(note_start, 3),
                    "end": round(float(t), 3),
                    "velocity": 80,
                    "confidence": 0.85
                })
                prev_midi = None

        if prev_midi is not None:
            melody_notes.append({
                "midi_note": prev_midi,
                "start": round(note_start, 3),
                "end": round(float(times[-1]), 3),
                "velocity": 80,
                "confidence": 0.85
            })

        return {
            "status": "ok",
            "audio_source_id": request.get("audio_source_id"),
            "analysis_type": "melody_extraction",
            "melody": melody_notes[-64:] if len(melody_notes) > 64 else melody_notes,
            "detected_scale": "auto"
        }


class ChordExtractor(AnalyzerAgent):
    def analyze(self, y: np.ndarray, sr: float,
                request: Dict[str, Any]) -> Dict[str, Any]:
        chroma = librosa.feature.chroma_cqt(y=y, sr=sr)
        hop_length = 512
        frame_duration = hop_length / sr

        chord_templates = self._build_chord_templates()
        n_frames = chroma.shape[1]
        chords = []
        current_chord = None
        chord_start = 0.0

        step = max(1, int(sr * 0.5 / hop_length))

        for i in range(0, n_frames, step):
            frame = chroma[:, i]
            frame_norm = frame / (np.sum(frame) + 1e-9)
            best_chord, best_corr = self._match_chord(frame_norm, chord_templates)
            t = i * frame_duration

            if best_chord != current_chord:
                if current_chord is not None:
                    chords.append({
                        "symbol": current_chord,
                        "start": round(chord_start, 2),
                        "end": round(t, 2),
                        "confidence": round(float(best_corr), 2)
                    })
                current_chord = best_chord
                chord_start = t

        if current_chord is not None and chords:
            chords[-1]["end"] = round(n_frames * frame_duration, 2)
        elif current_chord is not None:
            chords.append({
                "symbol": current_chord,
                "start": round(chord_start, 2),
                "end": round(n_frames * frame_duration, 2),
                "confidence": 0.5
            })

        return {
            "status": "ok",
            "audio_source_id": request.get("audio_source_id"),
            "analysis_type": "chord_extraction",
            "chords": chords,
            "key_estimate": "auto",
            "chord_density": round(len(chords) / max(1, n_frames * frame_duration), 2)
        }

    @staticmethod
    def _build_chord_templates() -> Dict[str, np.ndarray]:
        note_names = ['C', 'C#', 'D', 'D#', 'E', 'F',
                      'F#', 'G', 'G#', 'A', 'A#', 'B']
        templates = {}
        major_intervals = [0, 4, 7]
        minor_intervals = [0, 3, 7]

        for i, root in enumerate(note_names):
            major = np.zeros(12)
            for interval in major_intervals:
                major[(i + interval) % 12] = 1.0
            templates[f"{root}"] = major
            templates[f"{root}m"] = np.zeros(12)
            for interval in minor_intervals:
                templates[f"{root}m"][(i + interval) % 12] = 1.0

        return templates

    @staticmethod
    def _match_chord(frame: np.ndarray,
                     templates: Dict[str, np.ndarray]) -> tuple:
        best_name = "N"
        best_corr = -1.0
        for name, template in templates.items():
            t_norm = template / (np.sum(template) + 1e-9)
            corr = np.corrcoef(frame, t_norm)[0, 1]
            if corr > best_corr:
                best_corr = corr
                best_name = name
        if best_corr < 0.5:
            return "N", best_corr
        return best_name, best_corr


class SeparationAnalyzer(AnalyzerAgent):
    def analyze(self, y: np.ndarray, sr: float,
                request: Dict[str, Any]) -> Dict[str, Any]:
        S = np.abs(librosa.stft(y))
        spectral_bandwidth = librosa.feature.spectral_bandwidth(S=S, sr=sr)[0]
        rms = librosa.feature.rms(S=S)[0]
        avg_bw = float(np.mean(spectral_bandwidth))
        avg_rms = float(np.mean(rms))

        vocal_score = min(1.0, max(0.0, 0.3 + avg_bw / 8000))
        drums_score = min(1.0, max(0.0, 0.4 + (1.0 - avg_bw / 8000) * 0.5))
        bass_score = min(1.0, max(0.0, 0.35 + avg_rms * 2))
        other_score = min(1.0, max(0.0, 0.3 + avg_rms))

        scores = {
            "vocals": round(vocal_score, 2),
            "drums": round(drums_score, 2),
            "bass": round(bass_score, 2),
            "other": round(other_score, 2)
        }
        best = max(scores, key=scores.get)

        return {
            "status": "ok",
            "audio_source_id": request.get("audio_source_id"),
            "analysis_type": "separation_potential",
            "separation_scores": scores,
            "recommendations": {
                "best_stem": best,
                "difficulty": "medium",
                "suggested_model": "demucs_v4",
                "expected_quality": "good"
            },
            "confidence": round(float(np.mean(list(scores.values()))), 2)
        }


class GrooveAnalyzer(AnalyzerAgent):
    def analyze(self, y: np.ndarray, sr: float,
                request: Dict[str, Any]) -> Dict[str, Any]:
        tempo, beat_frames = librosa.beat.beat_track(y=y, sr=sr)
        if isinstance(tempo, np.ndarray):
            tempo = float(tempo[0])
        else:
            tempo = float(tempo)

        onset_env = librosa.onset.onset_strength(y=y, sr=sr)
        onset_frames = librosa.onset.onset_detect(onset_envelope=onset_env, sr=sr)

        if len(beat_frames) > 1:
            beat_times = librosa.frames_to_time(beat_frames, sr=sr)
            beat_intervals = np.diff(beat_times)
            timing_consistency = float(1.0 - np.std(beat_intervals) /
                                       (np.mean(beat_intervals) + 1e-9))
            timing_consistency = max(0.0, min(1.0, timing_consistency))
            tempo_stability = float(1.0 - np.std(beat_intervals) /
                                    (np.mean(beat_intervals) + 1e-9))
            tempo_stability = max(0.0, min(1.0, tempo_stability))
        else:
            timing_consistency = 0.5
            tempo_stability = 0.5

        hop_length = 512
        swing = 0.0
        if len(onset_frames) > 1:
            onset_times = librosa.frames_to_time(onset_frames, sr=sr, hop_length=hop_length)
            beat_period = 60.0 / (tempo + 1e-9)
            offsets = (onset_times % beat_period) / beat_period
            swing = float(np.mean(np.abs(offsets - 0.5)) * 2.0)
        swing_pct = 50.0 + swing * 16.0

        if len(beat_frames) >= 4:
            beat_strengths = []
            for bf in beat_frames[:4]:
                if bf < len(onset_env):
                    beat_strengths.append(float(onset_env[bf]) /
                                          (np.max(onset_env) + 1e-9))
            if len(beat_strengths) < 4:
                beat_strengths = [0.5] * 4
        else:
            beat_strengths = [0.5] * 4

        return {
            "status": "ok",
            "audio_source_id": request.get("audio_source_id"),
            "analysis_type": "groove",
            "groove": {
                "swing_percentage": round(swing_pct, 1),
                "timing_consistency": round(timing_consistency, 2),
                "humanization_amount": round(1.0 - timing_consistency, 2),
                "detected_tempo": round(tempo, 1),
                "tempo_stability": round(tempo_stability, 2),
                "beat_strength": [round(s, 2) for s in beat_strengths],
                "syncopation_level": round(swing, 2)
            },
            "confidence": round(timing_consistency, 2)
        }


class DynamicsAnalyzer(AnalyzerAgent):
    def analyze(self, y: np.ndarray, sr: float,
                request: Dict[str, Any]) -> Dict[str, Any]:
        rms = librosa.feature.rms(y=y)[0]
        rms_db = librosa.amplitude_to_db(rms, ref=1.0)

        S = np.abs(librosa.stft(y))
        peak_amp = np.max(S, axis=0)
        peak_db = librosa.amplitude_to_db(peak_amp, ref=1.0)

        dynamic_range = float(np.percentile(peak_db, 95) -
                              np.percentile(peak_db, 5))
        loudness_lufs = float(np.mean(rms_db) - 0.691)
        true_peak = float(np.max(peak_db))
        crest_factor = float(np.mean(peak_db - rms_db))

        onset_env = librosa.onset.onset_strength(y=y, sr=sr)
        if np.max(onset_env) > 0:
            onset_norm = onset_env / np.max(onset_env)
            transient_consistency = float(1.0 - np.std(onset_norm) /
                                          (np.mean(onset_norm) + 1e-9))
            transient_consistency = max(0.0, min(1.0, transient_consistency))
        else:
            transient_consistency = 0.5

        loudness_range = float(np.percentile(rms_db, 95) -
                               np.percentile(rms_db, 5))

        threshold = float(np.percentile(peak_db, 75))
        ratio = 4.0 if dynamic_range > 15 else 3.0 if dynamic_range > 10 else 2.0
        attack = 5.0 if transient_consistency > 0.7 else 10.0
        release = 80.0

        return {
            "status": "ok",
            "audio_source_id": request.get("audio_source_id"),
            "analysis_type": "dynamics",
            "dynamics": {
                "dynamic_range_db": round(dynamic_range, 1),
                "loudness_lufs": round(loudness_lufs, 1),
                "true_peak_db": round(true_peak, 1),
                "crest_factor_db": round(crest_factor, 1),
                "transient_consistency": round(transient_consistency, 2),
                "compression_suggested": {
                    "threshold": round(threshold, 1),
                    "ratio": ratio,
                    "attack": attack,
                    "release": release
                },
                "loudness_range_lu": round(loudness_range, 1),
                "peaks_above_threshold": int(np.sum(peak_db > threshold))
            },
            "confidence": 0.85
        }


class ProcessingSuggester(AnalyzerAgent):
    def analyze(self, y: np.ndarray, sr: float,
                request: Dict[str, Any]) -> Dict[str, Any]:
        S = np.abs(librosa.stft(y))
        spectral_centroids = librosa.feature.spectral_centroid(S=S, sr=sr)[0]
        rms = librosa.feature.rms(S=S)[0]

        avg_centroid = float(np.mean(spectral_centroids))
        avg_rms = float(np.mean(rms))

        eq_bands = [
            {"band": 1, "frequency": 60, "gain": round(-1.0 if avg_rms > 0.05 else 1.0, 1), "q": 1.0},
            {"band": 2, "frequency": 200, "gain": 1.5, "q": 1.2},
            {"band": 3, "frequency": 800, "gain": round(-1.5 if avg_centroid < 2000 else -0.5, 1), "q": 0.8},
            {"band": 4, "frequency": 3000, "gain": round(2.0 if avg_centroid < 4000 else 1.0, 1), "q": 1.0},
            {"band": 5, "frequency": 12000, "gain": round(1.5 if avg_centroid > 3000 else 0.5, 1), "q": 1.2}
        ]

        peak_db = librosa.amplitude_to_db(np.max(S, axis=0), ref=1.0)
        dynamic_range = float(np.percentile(peak_db, 95) - np.percentile(peak_db, 5))

        dynamics_suggestion = {
            "threshold": round(float(np.percentile(peak_db, 70)), 1),
            "ratio": 3.5 if dynamic_range > 12 else 2.5,
            "attack": 10.0 if dynamic_range > 15 else 5.0,
            "release": 100.0,
            "makeup_gain": round(max(0.0, 3.0 - dynamic_range * 0.15), 1)
        }

        return {
            "status": "ok",
            "audio_source_id": request.get("audio_source_id"),
            "analysis_type": "processing_suggestions",
            "suggestions": {
                "eq": eq_bands,
                "dynamics": dynamics_suggestion,
                "confidence": 0.75
            }
        }


class AraPluginCommunicator:
    """
    Communicates with ARA 2.0 host extensions.
    Handles offline analysis requests and metadata exchange.
    """

    def __init__(self, ipc_name: str = "maaps_ara_ipc"):
        self.ipc_name = ipc_name
        self.running = False
        self.request_queue = queue.Queue()
        self.response_queue = queue.Queue()
        self.processing_thread: Optional[threading.Thread] = None

        self.audio_loader = AudioLoader()

        self._agents: Dict[str, AnalyzerAgent] = {
            "analyze_spectral": SpectralAnalyzer(),
            "analyze_content": ContentAnalyzer(),
            "extract_melody": MelodyExtractor(),
            "extract_chords": ChordExtractor(),
            "analyze_separation_potential": SeparationAnalyzer(),
            "analyze_groove": GrooveAnalyzer(),
            "analyze_dynamics": DynamicsAnalyzer(),
            "get_processing_suggestions": ProcessingSuggester(),
        }

        logger.info(f"ARA Communicator initialized with IPC name: {ipc_name}")

    def start(self):
        if self.running:
            logger.warning("ARA Communicator already running")
            return

        self.running = True
        try:
            thread = threading.Thread(target=self._ipc_loop, daemon=True)
            if thread is None:
                raise RuntimeError("Failed to create ARA processing thread")
            self.processing_thread = thread
            self.processing_thread.start()
            logger.info("ARA Communicator started")
        except Exception as e:
            logger.error(f"Failed to start ARA Communicator: {e}")
            self.running = False
            self.processing_thread = None
            raise

    def stop(self):
        self.running = False
        thread = self.processing_thread
        if thread and thread.is_alive():
            try:
                thread.join(timeout=1.0)
            except Exception as e:
                logger.error(f"Error joining ARA thread: {e}")
        self.processing_thread = None
        logger.info("ARA Communicator stopped")

    def _ipc_loop(self):
        logger.info("ARA IPC loop started")
        while self.running:
            try:
                if not self.request_queue.empty():
                    req = self.request_queue.get()
                    self._handle_request(req)
                time.sleep(0.01)
            except Exception as e:
                logger.error(f"Error in ARA IPC loop: {e}")

    def _handle_request(self, request: Dict[str, Any]) -> None:
        req_type = request.get("type")
        logger.info(f"Received ARA request: {req_type}")

        response = {"status": "error", "message": "Unknown request"}

        if req_type == "analyze_region":
            response = self._handle_analyze_region(request)
        elif req_type == "get_metadata":
            response = self._handle_get_metadata(request)
        elif req_type == "handshake":
            response = {"status": "ok", "version": "1.0.0",
                        "capabilities": self.get_capabilities()}
        elif req_type in self._agents:
            response = self._dispatch_to_agent(req_type, request)

        self.response_queue.put(response)

    def _dispatch_to_agent(self, req_type: str,
                           request: Dict[str, Any]) -> Dict[str, Any]:
        agent = self._agents.get(req_type)
        if agent is None:
            return {"status": "error",
                    "message": f"No agent for request type: {req_type}"}

        audio_source_id = request.get("audio_source_id")
        start = request.get("start", 0.0)
        duration = request.get("duration", 0.0)

        result = self.audio_loader.load(audio_source_id, start, duration)
        if result is None:
            return {"status": "error",
                    "message": f"Could not load audio: {audio_source_id}"}

        y, sr = result
        if len(y) == 0:
            return {"status": "error", "message": "Audio segment is empty"}

        try:
            return agent.analyze(y, sr, request)
        except Exception as e:
            logger.error(f"Agent {req_type} failed: {e}")
            return {"status": "error", "message": str(e)}

    def get_capabilities(self) -> list:
        return [
            "spectral", "transient", "metadata", "melody_extraction",
            "chord_extraction", "content_analysis", "processing_suggestions",
            "separation_analysis", "groove_analysis", "dynamics_analysis"
        ]

    def _handle_analyze_region(self, request: Dict[str, Any]) -> Dict[str, Any]:
        audio_source_id = request.get("audio_source_id")
        start_time = request.get("start", 0.0)
        duration = request.get("duration", 0.0)

        logger.info(f"Analyzing region: ID={audio_source_id}, "
                     f"Start={start_time}, Dur={duration}")

        result = self.audio_loader.load(audio_source_id, start_time, duration)
        if result is None:
            return {
                "status": "error",
                "message": f"Could not load audio source: {audio_source_id}"
            }

        y, sr = result
        if len(y) == 0:
            return {"status": "error", "message": "Audio segment is empty"}

        analyses = {}
        for name, agent in self._agents.items():
            try:
                analyses[name] = agent.analyze(y, sr, request)
            except Exception as e:
                logger.warning(f"Agent {name} failed in region analysis: {e}")
                analyses[name] = {"status": "error", "message": str(e)}

        return {
            "status": "ok",
            "job_id": f"job_{int(time.time())}",
            "audio_source_id": audio_source_id,
            "message": "Analysis complete",
            "analyses": {k: v for k, v in analyses.items()
                         if v.get("status") == "ok"},
            "errors": {k: v for k, v in analyses.items()
                       if v.get("status") != "ok"}
        }

    def _handle_get_metadata(self, request: Dict[str, Any]) -> Dict[str, Any]:
        audio_source_id = request.get("audio_source_id")

        result = self.audio_loader.load(audio_source_id)
        if result is None:
            return {"status": "error",
                    "message": f"Could not load audio: {audio_source_id}"}

        y, sr = result
        duration = len(y) / sr
        rms = float(np.mean(librosa.feature.rms(y=y)))
        tempo, _ = librosa.beat.beat_track(y=y, sr=sr)
        if isinstance(tempo, np.ndarray):
            tempo = float(tempo[0])
        else:
            tempo = float(tempo)
        chroma = librosa.feature.chroma_cqt(y=y, sr=sr)
        key, scale = ContentAnalyzer._estimate_key(chroma)

        return {
            "status": "ok",
            "audio_source_id": audio_source_id,
            "metadata": {
                "duration": round(duration, 2),
                "sample_rate": sr,
                "tempo": round(tempo, 1),
                "key": key,
                "loudness": round(float(librosa.amplitude_to_db(
                    np.array([rms]), ref=1.0)[0]), 1)
            }
        }

    def send_mock_request(self, request: Dict[str, Any]):
        self.request_queue.put(request)

    def get_mock_response(self, timeout: float = 1.0) -> Optional[Dict[str, Any]]:
        try:
            return self.response_queue.get(timeout=timeout)
        except queue.Empty:
            return None
