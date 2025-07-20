"""Backend abstraction for code generation."""

from abc import ABC, abstractmethod
from typing import Dict, List, Optional, Any
import re


class CodeGenerationBackend(ABC):
    """Abstract base class for code generation backends."""
    
    @abstractmethod
    def generate(self, prompt: str, max_tokens: int = 512, temperature: float = 0.1,
                top_p: float = 0.95, stop_tokens: Optional[List[str]] = None) -> str:
        """
        Generate code completion for a given prompt.
        
        Args:
            prompt: Code prompt
            max_tokens: Maximum tokens to generate
            temperature: Sampling temperature (0.0 = deterministic)
            top_p: Top-p sampling parameter
            stop_tokens: List of stop tokens
            
        Returns:
            Generated code completion
        """
        pass
    
    @abstractmethod
    def batch_generate(self, prompts: List[str], max_tokens: int = 512, 
                      temperature: float = 0.1, top_p: float = 0.95,
                      stop_tokens: Optional[List[str]] = None) -> List[str]:
        """
        Generate code completions for multiple prompts.
        
        Args:
            prompts: List of code prompts
            max_tokens: Maximum tokens to generate
            temperature: Sampling temperature (0.0 = deterministic)
            top_p: Top-p sampling parameter
            stop_tokens: List of stop tokens
            
        Returns:
            List of generated code completions
        """
        pass


class VLLMBackend(CodeGenerationBackend):
    """VLLM-based code generation backend."""
    
    def __init__(self, model_name: str, tensor_parallel_size: int = 1, **kwargs):
        """
        Initialize VLLM backend.
        
        Args:
            model_name: HuggingFace model name or path
            tensor_parallel_size: Number of GPUs for tensor parallelism
            **kwargs: Additional VLLM configuration
        """
        # Import VLLM only when this backend is actually used
        try:
            from vllm import LLM, SamplingParams
            self._SamplingParams = SamplingParams
        except ImportError:
            raise ImportError("VLLM is required for VLLMBackend. Install with: pip install vllm")
        
        self.model_name = model_name
        self.llm = LLM(
            model=model_name,
            tensor_parallel_size=tensor_parallel_size,
            trust_remote_code=kwargs.get('trust_remote_code', True),
            gpu_memory_utilization=kwargs.get('gpu_memory_utilization', 0.9),
            max_model_len=kwargs.get('max_model_len', 8192),
            **{k: v for k, v in kwargs.items() if k not in ['trust_remote_code', 'gpu_memory_utilization', 'max_model_len']}
        )
    
    def generate(self, prompt: str, max_tokens: int = 512, temperature: float = 0.1,
                top_p: float = 0.95, stop_tokens: Optional[List[str]] = None) -> str:
        """Generate single code completion."""
        if stop_tokens is None:
            stop_tokens = ["\n\n\n", "```"]
        
        sampling_params = self._SamplingParams(
            max_tokens=max_tokens,
            temperature=temperature,
            top_p=top_p,
            stop=stop_tokens
        )
        
        outputs = self.llm.generate([prompt], sampling_params)
        return outputs[0].outputs[0].text
    
    def batch_generate(self, prompts: List[str], max_tokens: int = 512, 
                      temperature: float = 0.1, top_p: float = 0.95,
                      stop_tokens: Optional[List[str]] = None) -> List[str]:
        """Generate multiple code completions."""
        if stop_tokens is None:
            stop_tokens = ["\n\n", "def ", "class ", "import ", "from "]
        
        sampling_params = self._SamplingParams(
            max_tokens=max_tokens,
            temperature=temperature,
            top_p=top_p,
            stop=stop_tokens
        )
        
        outputs = self.llm.generate(prompts, sampling_params)
        return [output.outputs[0].text for output in outputs]


class MockBackend(CodeGenerationBackend):
    """Mock backend for testing with predictable responses."""
    
    def __init__(self, responses: Optional[Dict[str, str]] = None):
        """
        Initialize mock backend.
        
        Args:
            responses: Dictionary mapping prompt patterns to responses
        """
        self.responses = responses or self._default_responses()
        self.generation_count = 0
    
    def _default_responses(self) -> Dict[str, str]:
        """Default mock responses for common prompts."""
        return {
            "def fibonacci": """def fibonacci(n):
    if n <= 1:
        return n
    return fibonacci(n-1) + fibonacci(n-2)""",
            
            "bool isPrime": """bool isPrime(int n) {
    if (n <= 1) return false;
    for (int i = 2; i * i <= n; i++) {
        if (n % i == 0) return false;
    }
    return true;
}""",
            
            "function isPrime": """function isPrime(n) {
    if (n <= 1) return false;
    for (let i = 2; i * i <= n; i++) {
        if (n % i === 0) return false;
    }
    return true;
}""",
            
            "public static boolean isPrime": """public static boolean isPrime(int n) {
    if (n <= 1) return false;
    for (int i = 2; i * i <= n; i++) {
        if (n % i == 0) return false;
    }
    return true;
}""",
            
            "def is_prime": """def is_prime(n):
    if n <= 1:
        return False
    for i in range(2, int(n ** 0.5) + 1):
        if n % i == 0:
            return False
    return True""",
            
            "def test": """def test():
    \"\"\"Test function.\"\"\"
    pass""",
            
            "class": """class Calculator:
    \"\"\"Simple calculator class.\"\"\"
    
    def add(self, a, b):
        return a + b""",
        }
    
    def generate(self, prompt: str, max_tokens: int = 512, temperature: float = 0.1,
                top_p: float = 0.95, stop_tokens: Optional[List[str]] = None) -> str:
        """Generate single mock response."""
        self.generation_count += 1
        
        # Find matching response pattern
        prompt_lower = prompt.lower().strip()
        for pattern, response in self.responses.items():
            if pattern.lower() in prompt_lower:
                return response
        
        # Default response for unknown prompts
        return f"""# Generated code for: {prompt.strip()}
# Iteration: {self.generation_count}
pass"""
    
    def batch_generate(self, prompts: List[str], max_tokens: int = 512, 
                      temperature: float = 0.1, top_p: float = 0.95,
                      stop_tokens: Optional[List[str]] = None) -> List[str]:
        """Generate multiple mock responses."""
        return [self.generate(prompt, max_tokens, temperature, top_p, stop_tokens) 
                for prompt in prompts]
    
    def add_response(self, pattern: str, response: str):
        """Add custom response pattern."""
        self.responses[pattern] = response
    
    def reset_count(self):
        """Reset generation counter."""
        self.generation_count = 0


class FileBackend(CodeGenerationBackend):
    """Backend that reads responses from files (useful for testing with real examples)."""
    
    def __init__(self, responses_file: str):
        """
        Initialize file-based backend.
        
        Args:
            responses_file: Path to JSON file with prompt->response mappings
        """
        import json
        from pathlib import Path
        
        self.responses_file = Path(responses_file)
        if self.responses_file.exists():
            with open(self.responses_file) as f:
                self.responses = json.load(f)
        else:
            self.responses = {}
    
    def generate(self, prompt: str, max_tokens: int = 512, temperature: float = 0.1,
                top_p: float = 0.95, stop_tokens: Optional[List[str]] = None) -> str:
        """Generate response from file."""
        prompt_key = prompt.strip()
        return self.responses.get(prompt_key, f"# No response found for: {prompt}")
    
    def batch_generate(self, prompts: List[str], max_tokens: int = 512, 
                      temperature: float = 0.1, top_p: float = 0.95,
                      stop_tokens: Optional[List[str]] = None) -> List[str]:
        """Generate multiple responses from file."""
        return [self.generate(prompt, max_tokens, temperature, top_p, stop_tokens) 
                for prompt in prompts]