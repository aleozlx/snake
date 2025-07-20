"""Minimal tests for basic functionality."""

import pytest
import sys
import os
from pathlib import Path

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))


@pytest.mark.smoke
def test_basic_imports():
    """Test that basic imports work."""
    try:
        import common
        assert hasattr(common, 'NaturalLanguageProcessor')
        assert hasattr(common, 'setup_cuda_environment')
    except ImportError as e:
        pytest.fail(f"Failed to import common module: {e}")


@pytest.mark.smoke
def test_natural_language_detection():
    """Test natural language detection."""
    from common import NaturalLanguageProcessor
    
    # Natural language cases
    assert NaturalLanguageProcessor.is_natural_language_prompt("create a function") == True
    assert NaturalLanguageProcessor.is_natural_language_prompt("write a program") == True
    assert NaturalLanguageProcessor.is_natural_language_prompt("implement sorting") == True
    
    # Code cases
    assert NaturalLanguageProcessor.is_natural_language_prompt("def test():") == False
    assert NaturalLanguageProcessor.is_natural_language_prompt("bool isPrime(int n) {") == False
    assert NaturalLanguageProcessor.is_natural_language_prompt("import numpy") == False


@pytest.mark.smoke
def test_natural_language_conversion():
    """Test natural language to code conversion."""
    from common import NaturalLanguageProcessor
    
    # Test C++ conversion
    result = NaturalLanguageProcessor.convert_nl_to_code_prompt("write a prime checker in c++")
    assert "bool isPrime" in result
    
    # Test Python conversion
    result = NaturalLanguageProcessor.convert_nl_to_code_prompt("create a prime function in python")
    assert "def is_prime" in result
    
    # Test Java conversion
    result = NaturalLanguageProcessor.convert_nl_to_code_prompt("implement prime in java")
    assert "public static boolean isPrime" in result
    
    # Test JavaScript conversion
    result = NaturalLanguageProcessor.convert_nl_to_code_prompt("make prime function in javascript")
    assert "function isPrime" in result


@pytest.mark.smoke
def test_cuda_setup():
    """Test CUDA environment setup."""
    from common import setup_cuda_environment
    
    # Clear environment variable if it exists
    if "CUDA_VISIBLE_DEVICES" in os.environ:
        del os.environ["CUDA_VISIBLE_DEVICES"]
        
    setup_cuda_environment()
    assert os.environ.get("CUDA_VISIBLE_DEVICES") == "0"


@pytest.mark.smoke
def test_argument_parser():
    """Test argument parser creation."""
    from common import create_argument_parser
    
    parser = create_argument_parser("Test Description")
    help_text = parser.format_help()
    
    # Check that all expected arguments are present
    assert "--model" in help_text
    assert "--max-tokens" in help_text
    assert "--temperature" in help_text
    assert "--top-p" in help_text
    assert "--tensor-parallel" in help_text


@pytest.mark.smoke
def test_constants_exist():
    """Test that required constants exist."""
    from common import DEFAULT_MODELS, STOP_TOKENS
    
    assert isinstance(DEFAULT_MODELS, dict)
    assert isinstance(STOP_TOKENS, dict)
    assert len(DEFAULT_MODELS) > 0
    assert len(STOP_TOKENS) > 0


@pytest.mark.smoke
def test_file_structure():
    """Test that all required files exist."""
    agent_dir = Path(__file__).parent.parent
    
    required_files = ['a1.py', 'a2.py', 'common.py', 'backend.py', 'factory.py', 'run_agent.py']
    for filename in required_files:
        filepath = agent_dir / filename
        assert filepath.exists(), f"Missing required file: {filename}"
        assert filepath.is_file(), f"{filename} is not a file"


if __name__ == "__main__":
    pytest.main([__file__, "-v", "-m", "smoke"])