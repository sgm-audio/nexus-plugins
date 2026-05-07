"""
Test Script for ARA Extensions
Tests all the newly added ARA 2.0 functionality
"""
import logging
import time
from src.daw_integration.ara_wrapper import AraPluginCommunicator

def test_all_ara_extensions():
    logging.basicConfig(level=logging.INFO)
    logger = logging.getLogger("TestARAExtensions")
    
    logger.info("Starting Comprehensive ARA Extensions Test...")
    
    communicator = AraPluginCommunicator(ipc_name="maaps_test_ipc_ext")
    communicator.start()
    
    test_results = {}
    
    try:
        # Test 1: Handshake (should already work)
        logger.info("Test 1: Handshake")
        communicator.send_mock_request({"type": "handshake"})
        resp = communicator.get_mock_response(timeout=1.0)
        test_results["handshake"] = resp is not None and resp.get("status") == "ok"
        logger.info(f"Handshake: {'PASS' if test_results['handshake'] else 'FAIL'}")
        
        # Test 2: Analyze Region (should already work)
        logger.info("Test 2: Analyze Region")
        communicator.send_mock_request({
            "type": "analyze_region",
            "audio_source_id": "track_123",
            "start": 0.0,
            "duration": 10.0
        })
        resp = communicator.get_mock_response(timeout=1.0)
        test_results["analyze_region"] = resp is not None and resp.get("status") == "ok"
        logger.info(f"Analyze Region: {'PASS' if test_results['analyze_region'] else 'FAIL'}")
        
        # Test 3: Spectral Analysis
        logger.info("Test 3: Spectral Analysis")
        communicator.send_mock_request({
            "type": "analyze_spectral",
            "audio_source_id": "track_123",
            "start": 0.0,
            "duration": 5.0
        })
        resp = communicator.get_mock_response(timeout=2.0)
        test_results["spectral"] = resp is not None and resp.get("status") == "ok" and "data" in resp
        logger.info(f"Spectral Analysis: {'PASS' if test_results['spectral'] else 'FAIL'}")
        
        # Test 4: Processing Suggestions
        logger.info("Test 4: Processing Suggestions")
        communicator.send_mock_request({
            "type": "get_processing_suggestions",
            "audio_source_id": "track_123",
            "start": 0.0,
            "duration": 5.0
        })
        resp = communicator.get_mock_response(timeout=2.0)
        test_results["processing_suggestions"] = resp is not None and resp.get("status") == "ok" and "suggestions" in resp
        logger.info(f"Processing Suggestions: {'PASS' if test_results['processing_suggestions'] else 'FAIL'}")
        
        # Test 5: Content Analysis
        logger.info("Test 5: Content Analysis")
        communicator.send_mock_request({
            "type": "analyze_content",
            "audio_source_id": "track_123",
            "start": 0.0,
            "duration": 10.0
        })
        resp = communicator.get_mock_response(timeout=2.0)
        test_results["content"] = resp is not None and resp.get("status") == "ok" and "content" in resp
        logger.info(f"Content Analysis: {'PASS' if test_results['content'] else 'FAIL'}")
        
        # Test 6: Melody Extraction
        logger.info("Test 6: Melody Extraction")
        communicator.send_mock_request({
            "type": "extract_melody",
            "audio_source_id": "track_123",
            "start": 0.0,
            "duration": 5.0
        })
        resp = communicator.get_mock_response(timeout=2.0)
        test_results["melody"] = resp is not None and resp.get("status") == "ok" and "melody" in resp
        logger.info(f"Melody Extraction: {'PASS' if test_results['melody'] else 'FAIL'}")
        
        # Test 7: Chord Extraction
        logger.info("Test 7: Chord Extraction")
        communicator.send_mock_request({
            "type": "extract_chords",
            "audio_source_id": "track_123",
            "start": 0.0,
            "duration": 8.0
        })
        resp = communicator.get_mock_response(timeout=2.0)
        test_results["chords"] = resp is not None and resp.get("status") == "ok" and "chords" in resp
        logger.info(f"Chord Extraction: {'PASS' if test_results['chords'] else 'FAIL'}")
        
        # Test 8: Separation Potential
        logger.info("Test 8: Separation Potential")
        communicator.send_mock_request({
            "type": "analyze_separation_potential",
            "audio_source_id": "track_123",
            "start": 0.0,
            "duration": 10.0
        })
        resp = communicator.get_mock_response(timeout=2.0)
        test_results["separation"] = resp is not None and resp.get("status") == "ok" and "separation_scores" in resp
        logger.info(f"Separation Potential: {'PASS' if test_results['separation'] else 'FAIL'}")
        
        # Test 9: Groove Analysis
        logger.info("Test 9: Groove Analysis")
        communicator.send_mock_request({
            "type": "analyze_groove",
            "audio_source_id": "track_123",
            "start": 0.0,
            "duration": 10.0
        })
        resp = communicator.get_mock_response(timeout=2.0)
        test_results["groove"] = resp is not None and resp.get("status") == "ok" and "groove" in resp
        logger.info(f"Groove Analysis: {'PASS' if test_results['groove'] else 'FAIL'}")
        
        # Test 10: Dynamics Analysis
        logger.info("Test 10: Dynamics Analysis")
        communicator.send_mock_request({
            "type": "analyze_dynamics",
            "audio_source_id": "track_123",
            "start": 0.0,
            "duration": 10.0
        })
        resp = communicator.get_mock_response(timeout=2.0)
        test_results["dynamics"] = resp is not None and resp.get("status") == "ok" and "dynamics" in resp
        logger.info(f"Dynamics Analysis: {'PASS' if test_results['dynamics'] else 'FAIL'}")
        
    finally:
        communicator.stop()
        logger.info("ARA Extensions Test completed.")
    
    # Summary
    passed = sum(test_results.values())
    total = len(test_results)
    logger.info(f"\n=== TEST SUMMARY ===")
    logger.info(f"Passed: {passed}/{total}")
    
    for test_name, result in test_results.items():
        logger.info(f"{test_name}: {'PASS' if result else 'FAIL'}")
    
    if passed == total:
        logger.info("🎉 ALL TESTS PASSED!")
        return True
    else:
        logger.warning(f"❌ {total - passed} TEST(S) FAILED")
        return False

if __name__ == "__main__":
    success = test_all_ara_extensions()
    exit(0 if success else 1)