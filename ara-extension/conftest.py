import numpy as np
import soundfile as sf
import pytest
from src.daw_integration.ara_wrapper import (
    AraPluginCommunicator,
    SpectralAnalyzer,
    ContentAnalyzer,
    MelodyExtractor,
    ChordExtractor,
    SeparationAnalyzer,
    GrooveAnalyzer,
    DynamicsAnalyzer,
    ProcessingSuggester,
    AudioLoader,
)


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
def audio_data():
    sr = 22050
    duration = 3.0
    t = np.linspace(0, duration, int(sr * duration), endpoint=False)
    y = 0.5 * np.sin(2 * np.pi * 440 * t) + 0.3 * np.sin(2 * np.pi * 880 * t)
    return y, sr


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
