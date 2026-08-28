import os
import tempfile
import numpy as np
import soundfile as sf
import pytest
from src.daw_integration.ara_wrapper import AraPluginCommunicator


@pytest.fixture
def audio_file(tmp_path):
    sr = 22050
    duration = 3.0
    t = np.linspace(0, duration, int(sr * duration), endpoint=False)
    y = 0.5 * np.sin(2 * np.pi * 440 * t) + 0.3 * np.sin(2 * np.pi * 880 * t)
    path = str(tmp_path / "test_audio.wav")
    sf.write(path, y, sr)
    return path


@pytest.fixture
def communicator():
    comm = AraPluginCommunicator(ipc_name="maaps_test_ipc")
    comm.start()
    yield comm
    comm.stop()


@pytest.fixture
def communicator_with_audio(communicator, audio_file):
    communicator.audio_loader._cache.clear()
    return communicator, audio_file


def _send_and_receive(comm, request, timeout=5.0):
    comm.send_mock_request(request)
    return comm.get_mock_response(timeout=timeout)


def test_handshake(communicator):
    resp = _send_and_receive(communicator, {"type": "handshake"})
    assert resp is not None
    assert resp["status"] == "ok"
    assert "version" in resp
    assert "capabilities" in resp


def test_analyze_region_with_audio(communicator_with_audio):
    comm, audio_path = communicator_with_audio
    resp = _send_and_receive(comm, {
        "type": "analyze_region",
        "audio_source_id": audio_path,
        "start": 0.0,
        "duration": 3.0,
    })
    assert resp is not None
    assert resp["status"] == "ok"
    assert "job_id" in resp
    assert "analyses" in resp


def test_analyze_region_bad_source(communicator):
    resp = _send_and_receive(communicator, {
        "type": "analyze_region",
        "audio_source_id": "/nonexistent/path.wav",
        "start": 0.0,
        "duration": 10.0,
    })
    assert resp is not None
    assert resp["status"] == "error"


def test_get_metadata_with_audio(communicator_with_audio):
    comm, audio_path = communicator_with_audio
    resp = _send_and_receive(comm, {
        "type": "get_metadata",
        "audio_source_id": audio_path,
    })
    assert resp is not None
    assert resp["status"] == "ok"
    assert "metadata" in resp
    meta = resp["metadata"]
    assert "duration" in meta
    assert "sample_rate" in meta


def test_spectral_analysis_with_audio(communicator_with_audio):
    comm, audio_path = communicator_with_audio
    resp = _send_and_receive(comm, {
        "type": "analyze_spectral",
        "audio_source_id": audio_path,
        "start": 0.0,
        "duration": 2.0,
    })
    assert resp is not None
    assert resp["status"] == "ok"
    assert "data" in resp
    assert "spectral_centroid" in resp["data"]
    assert "mfccs" in resp["data"]


def test_processing_suggestions_with_audio(communicator_with_audio):
    comm, audio_path = communicator_with_audio
    resp = _send_and_receive(comm, {
        "type": "get_processing_suggestions",
        "audio_source_id": audio_path,
        "start": 0.0,
        "duration": 2.0,
    })
    assert resp is not None
    assert resp["status"] == "ok"
    assert "suggestions" in resp
    assert "eq" in resp["suggestions"]
    assert "dynamics" in resp["suggestions"]


def test_content_analysis_with_audio(communicator_with_audio):
    comm, audio_path = communicator_with_audio
    resp = _send_and_receive(comm, {
        "type": "analyze_content",
        "audio_source_id": audio_path,
        "start": 0.0,
        "duration": 2.0,
    })
    assert resp is not None
    assert resp["status"] == "ok"
    assert "content" in resp
    content = resp["content"]
    assert "key" in content
    assert "tempo_bpm" in content
    assert "energy" in content


def test_melody_extraction_with_audio(communicator_with_audio):
    comm, audio_path = communicator_with_audio
    resp = _send_and_receive(comm, {
        "type": "extract_melody",
        "audio_source_id": audio_path,
        "start": 0.0,
        "duration": 2.0,
    })
    assert resp is not None
    assert resp["status"] == "ok"
    assert "melody" in resp


def test_chord_extraction_with_audio(communicator_with_audio):
    comm, audio_path = communicator_with_audio
    resp = _send_and_receive(comm, {
        "type": "extract_chords",
        "audio_source_id": audio_path,
        "start": 0.0,
        "duration": 2.0,
    })
    assert resp is not None
    assert resp["status"] == "ok"
    assert "chords" in resp


def test_separation_potential_with_audio(communicator_with_audio):
    comm, audio_path = communicator_with_audio
    resp = _send_and_receive(comm, {
        "type": "analyze_separation_potential",
        "audio_source_id": audio_path,
        "start": 0.0,
        "duration": 2.0,
    })
    assert resp is not None
    assert resp["status"] == "ok"
    assert "separation_scores" in resp
    scores = resp["separation_scores"]
    assert "vocals" in scores
    assert "drums" in scores


def test_groove_analysis_with_audio(communicator_with_audio):
    comm, audio_path = communicator_with_audio
    resp = _send_and_receive(comm, {
        "type": "analyze_groove",
        "audio_source_id": audio_path,
        "start": 0.0,
        "duration": 2.0,
    })
    assert resp is not None
    assert resp["status"] == "ok"
    assert "groove" in resp
    groove = resp["groove"]
    assert "detected_tempo" in groove
    assert "timing_consistency" in groove


def test_dynamics_analysis_with_audio(communicator_with_audio):
    comm, audio_path = communicator_with_audio
    resp = _send_and_receive(comm, {
        "type": "analyze_dynamics",
        "audio_source_id": audio_path,
        "start": 0.0,
        "duration": 2.0,
    })
    assert resp is not None
    assert resp["status"] == "ok"
    assert "dynamics" in resp
    dyn = resp["dynamics"]
    assert "dynamic_range_db" in dyn
    assert "compression_suggested" in dyn


def test_unknown_request_type(communicator):
    resp = _send_and_receive(communicator, {"type": "nonexistent"})
    assert resp is not None
    assert resp["status"] == "error"
