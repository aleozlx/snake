"""Factory functions for creating agents with different backends."""

import os
from typing import Optional, Dict, Any, Union
from backend import CodeGenerationBackend, VLLMBackend, MockBackend, FileBackend


def create_backend(backend_type: str = "vllm", **kwargs) -> CodeGenerationBackend:
    """
    Create a code generation backend.
    
    Args:
        backend_type: Type of backend ("vllm", "mock", "file")
        **kwargs: Backend-specific configuration
        
    Returns:
        Configured backend instance
    """
    if backend_type.lower() == "vllm":
        model_name = kwargs.get("model_name", "codellama/CodeLlama-7b-Python-hf")
        tensor_parallel_size = kwargs.get("tensor_parallel_size", 1)
        
        # Set up CUDA environment
        if "CUDA_VISIBLE_DEVICES" not in os.environ:
            os.environ["CUDA_VISIBLE_DEVICES"] = "0"
        
        return VLLMBackend(
            model_name=model_name,
            tensor_parallel_size=tensor_parallel_size,
            **{k: v for k, v in kwargs.items() if k not in ["model_name", "tensor_parallel_size"]}
        )
    
    elif backend_type.lower() == "mock":
        responses = kwargs.get("responses", None)
        return MockBackend(responses=responses)
    
    elif backend_type.lower() == "file":
        responses_file = kwargs.get("responses_file")
        if not responses_file:
            raise ValueError("FileBackend requires 'responses_file' parameter")
        return FileBackend(responses_file)
    
    else:
        raise ValueError(f"Unknown backend type: {backend_type}. "
                        f"Supported types: vllm, mock, file")


def create_agent(agent_type: str = "a2", backend: Optional[CodeGenerationBackend] = None, 
                backend_type: str = "vllm", natural_language: bool = True, **kwargs):
    """
    Create an agent with the specified backend.
    
    Args:
        agent_type: Type of agent ("a1", "a2")
        backend: Pre-configured backend (if None, creates one using backend_type)
        backend_type: Type of backend to create if backend is None
        natural_language: Enable natural language processing for a2 agent (default: True)
        **kwargs: Additional arguments for backend or agent creation
        
    Returns:
        Configured agent instance
    """
    # Create backend if not provided
    if backend is None:
        backend = create_backend(backend_type, **kwargs)
    
    # Import and create the appropriate agent
    if agent_type.lower() == "a1":
        from a1 import VLLMCodeGenerator
        return VLLMCodeGenerator(backend)
    
    elif agent_type.lower() == "a2":
        from a2 import InteractiveCodeGenerator
        return InteractiveCodeGenerator(backend, natural_language=natural_language)
    
    else:
        raise ValueError(f"Unknown agent type: {agent_type}. "
                        f"Supported types: a1, a2")


def create_mock_agent(agent_type: str = "a2", responses: Optional[Dict[str, str]] = None, 
                     natural_language: bool = True):
    """
    Convenience function to create an agent with mock backend for testing.
    
    Args:
        agent_type: Type of agent ("a1", "a2")
        responses: Custom response dictionary for mock backend
        natural_language: Enable natural language processing for a2 agent (default: True)
        
    Returns:
        Agent with mock backend
    """
    mock_backend = MockBackend(responses=responses)
    return create_agent(agent_type, backend=mock_backend, natural_language=natural_language)


def create_vllm_agent(agent_type: str = "a2", model_name: str = "codellama/CodeLlama-7b-Python-hf",
                     **kwargs):
    """
    Convenience function to create an agent with VLLM backend.
    
    Args:
        agent_type: Type of agent ("a1", "a2", "a3")
        model_name: HuggingFace model name or path
        **kwargs: Additional VLLM configuration
        
    Returns:
        Agent with VLLM backend
    """
    vllm_backend = create_backend("vllm", model_name=model_name, **kwargs)
    return create_agent(agent_type, backend=vllm_backend)


# Convenience aliases for common configurations
def create_python_agent(backend_type: str = "mock", **kwargs):
    """Create agent optimized for Python code generation."""
    if backend_type == "mock":
        python_responses = {
            "def": "def example_function():\n    \"\"\"Example function.\"\"\"\n    pass",
            "class": "class ExampleClass:\n    \"\"\"Example class.\"\"\"\n    pass",
            "import": "import os\nimport sys",
            "function": "def function():\n    pass"
        }
        return create_mock_agent("a3", responses=python_responses)
    else:
        return create_agent("a3", backend_type=backend_type, **kwargs)


def create_cpp_agent(backend_type: str = "mock", **kwargs):
    """Create agent optimized for C++ code generation."""
    if backend_type == "mock":
        cpp_responses = {
            "bool": "bool function(int n) {\n    return true;\n}",
            "int": "int function() {\n    return 0;\n}",
            "class": "class Example {\npublic:\n    Example();\n};",
            "#include": "#include <iostream>\n#include <vector>"
        }
        return create_mock_agent("a3", responses=cpp_responses)
    else:
        return create_agent("a3", backend_type=backend_type, **kwargs)


# Configuration presets
PRESET_CONFIGS = {
    "fast": {
        "max_tokens": 256,
        "temperature": 0.0,
        "top_p": 1.0
    },
    "creative": {
        "max_tokens": 512,
        "temperature": 0.7,
        "top_p": 0.9
    },
    "precise": {
        "max_tokens": 128,
        "temperature": 0.0,
        "top_p": 1.0
    },
    "balanced": {
        "max_tokens": 512,
        "temperature": 0.1,
        "top_p": 0.95
    }
}


def create_agent_with_preset(agent_type: str = "a2", preset: str = "balanced", 
                           backend_type: str = "vllm", **kwargs):
    """
    Create agent with predefined configuration preset.
    
    Args:
        agent_type: Type of agent
        preset: Configuration preset ("fast", "creative", "precise", "balanced")
        backend_type: Type of backend
        **kwargs: Override any preset values
        
    Returns:
        Configured agent
    """
    if preset not in PRESET_CONFIGS:
        raise ValueError(f"Unknown preset: {preset}. Available: {list(PRESET_CONFIGS.keys())}")
    
    config = PRESET_CONFIGS[preset].copy()
    config.update(kwargs)  # Allow overrides
    
    return create_agent(agent_type, backend_type=backend_type, **config)


def list_available_models():
    """List commonly used models for VLLM backend."""
    return {
        "codellama-7b": "codellama/CodeLlama-7b-Python-hf",
        "codellama-13b": "codellama/CodeLlama-13b-Python-hf", 
        "wizardcoder": "WizardLM/WizardCoder-Python-7B-V1.0",
        "deepseek": "deepseek-ai/deepseek-coder-6.7b-instruct",
        "starcoder": "bigcode/starcoder",
        "codeqwen": "Qwen/CodeQwen1.5-7B"
    }


def create_agent_from_config(config_dict: Dict[str, Any]):
    """
    Create agent from configuration dictionary.
    
    Args:
        config_dict: Configuration with keys like agent_type, backend_type, etc.
        
    Returns:
        Configured agent
    """
    agent_type = config_dict.get("agent_type", "a2")
    backend_type = config_dict.get("backend_type", "vllm")
    
    # Extract backend config
    backend_config = {k: v for k, v in config_dict.items() 
                     if k not in ["agent_type", "backend_type"]}
    
    return create_agent(agent_type, backend_type=backend_type, **backend_config)