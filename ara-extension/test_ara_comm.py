import numpy as np
import soundfile as sf
import pytest
from src.daw_integration.ara_wrapper import AraPluginCommunicator


@pytest.fixture
def audio_file(tmp_path):
    sr = 22050
    duration = 3.0
    t = np.linspace(0, duration, int(sr * duration), endpoint=False)
    y = 0.5 * np.sin(2 * np.pi * 440 * t)
    path = str(tmp_path / "test_audio.wav")
    sf.write(path, y, sr)
    return path


@pytest.fixture
def communicator():
    comm = AraPluginCommunicator(ipc_name="maaps_test_ipc_comm")
    comm.start()
    yield comm
    comm.stop()


def _send_and_receive(comm, request, timeout=5.0):
    comm.send_mock_request(request)
    return comm.get_mock_response(timeout=timeout)


def test_handshake(communicator):
    resp = _send_and_receive(communicator, {"type": "handshake"})
    assert resp is not None
    assert resp["status"] == "ok"
    assert "version" in resp


def test_analyze_region_with_audio(communicator, audio_file):
    resp = _send_and_receive(communicator, {
        "type": "analyze_region",
        "audio_source_id": audio_file,
        "start": 0.0,
        "duration": 3.0,
    }, timeout=15.0)
    assert resp is not None
    assert resp["status"] == "ok"
    assert "job_id" in resp


def test_analyze_region_bad_source(communicator):
    resp = _send_and_receive(communicator, {
        "type": "analyze_region",
        "audio_source_id": "/nonexistent/audio.wav",
        "start": 0.0,
        "duration": 10.0,
    })
    assert resp is not None
    assert resp["status"] == "error"
