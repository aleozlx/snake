#!/usr/bin/env python3
"""
Common utilities for VLLM code generation agents.

This module contains shared functionality used across different code generation scripts.
"""

import os
import sys
from typing import List, Optional, Dict, Any
from vllm import LLM, SamplingParams


class VLLMBase:
    """Base class for VLLM code generators with common functionality."""
    
    def __init__(self, model_name: str, tensor_parallel_size: int = 1):
        """
        Initialize VLLM code generator.

        Args:
            model_name: HuggingFace model name or path
            tensor_parallel_size: Number of GPUs for tensor parallelism
        """
        self.model_name = model_name
        self.llm = LLM(
            model=model_name,
            tensor_parallel_size=tensor_parallel_size,
            trust_remote_code=True,
            gpu_memory_utilization=0.9,
            max_model_len=8192
        )

    def generate_code(
        self,
        prompt: str,
        max_tokens: int = 512,
        temperature: float = 0.1,
        top_p: float = 0.95,
        stop_tokens: Optional[List[str]] = None
    ) -> str:
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
        if stop_tokens is None:
            stop_tokens = ["\n\n\n", "```"]

        sampling_params = SamplingParams(
            max_tokens=max_tokens,
            temperature=temperature,
            top_p=top_p,
            stop=stop_tokens
        )

        outputs = self.llm.generate([prompt], sampling_params)
        return outputs[0].outputs[0].text

    def generate_code_batch(
        self,
        prompts: List[str],
        max_tokens: int = 512,
        temperature: float = 0.1,
        top_p: float = 0.95,
        stop_tokens: Optional[List[str]] = None
    ) -> List[str]:
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
        if stop_tokens is None:
            stop_tokens = ["\n\n", "def ", "class ", "import ", "from "]

        sampling_params = SamplingParams(
            max_tokens=max_tokens,
            temperature=temperature,
            top_p=top_p,
            stop=stop_tokens
        )

        outputs = self.llm.generate(prompts, sampling_params)
        return [output.outputs[0].text for output in outputs]


def setup_cuda_environment():
    """Set up CUDA environment if not already configured."""
    if "CUDA_VISIBLE_DEVICES" not in os.environ:
        os.environ["CUDA_VISIBLE_DEVICES"] = "0"


def get_input(prompt_text: str = "Enter your prompt") -> str:
    """
    Get single line input from user with keyboard interrupt handling.
    
    Args:
        prompt_text: Prompt message to display
        
    Returns:
        User input string
    """
    try:
        return input(f"{prompt_text}: ").strip()
    except KeyboardInterrupt:
        print("\nExiting...")
        sys.exit(0)


def display_code(code: str, iteration: int = None):
    """
    Display generated code with formatting.
    
    Args:
        code: Code to display
        iteration: Optional iteration number
    """
    separator = "=" * 60
    if iteration is not None:
        print(f"\n{separator}")
        print(f"ITERATION {iteration} - Generated Code:")
        print(f"{separator}")
    else:
        print(f"\n{separator}")
        print("Generated Code:")
        print(f"{separator}")
    print(code)
    print(f"{separator}")


class NaturalLanguageProcessor:
    """Utility class for processing natural language prompts."""
    
    @staticmethod
    def is_natural_language_prompt(text: str) -> bool:
        """
        Detect if the input is a natural language request vs code.
        
        Args:
            text: User input text
            
        Returns:
            True if it looks like natural language, False if it looks like code
        """
        text_lower = text.lower().strip()
        
        # Strong NL indicators - these suggest natural language
        strong_nl_patterns = [
            'create', 'write', 'make', 'build', 'implement', 'generate',
            'a function', 'a class', 'a program', 'a script',
            'function that', 'class that', 'program that',
            'how to', 'help me', 'can you'
        ]
        
        # Strong code indicators - these suggest actual code
        strong_code_patterns = [
            'def ', 'class ', 'int main', 'void ', 'public class',
            '#include', 'import ', 'from ', '{\n', '}\n',
            ');', 'return;'
        ]
        
        # Check for strong patterns first
        for pattern in strong_nl_patterns:
            if pattern in text_lower:
                return True
                
        for pattern in strong_code_patterns:
            if pattern in text_lower:
                return False
        
        # Additional heuristics
        # If it ends with "in [language]", it's likely NL
        if any(text_lower.endswith(f' in {lang}') for lang in ['python', 'c++', 'java', 'javascript', 'c', 'go', 'rust']):
            return True
            
        # Check for code-like structure patterns
        # C-style function signatures: type name(params) {
        if ' (' in text and (') {' in text or ')\\n{' in text):
            return False
            
        # If it has parentheses and curly braces together, likely code
        if '(' in text and ')' in text and '{' in text:
            return False
            
        # If it has no special characters commonly found in code, likely NL
        has_code_chars = any(char in text for char in [';', '#', '=', '&&', '||'])
        if not has_code_chars and len(text.split()) > 3:
            return True
            
        return False

    @staticmethod
    def convert_nl_to_code_prompt(nl_prompt: str) -> str:
        """
        Convert natural language prompt to a code-starting prompt.
        
        Args:
            nl_prompt: Natural language prompt
            
        Returns:
            Code-starting prompt that the model can complete
        """
        nl_lower = nl_prompt.lower()
        
        # Language-specific conversions
        if any(lang in nl_lower for lang in ['c++', 'cpp', 'c plus']):
            if 'prime' in nl_lower:
                return "bool isPrime(int n) {"
            elif 'function' in nl_lower:
                return "// " + nl_prompt + "\n"
        elif any(lang in nl_lower for lang in ['python', 'py']):
            if 'prime' in nl_lower:
                return "def is_prime(n):"
            else:
                return "def "
        elif any(lang in nl_lower for lang in ['java']) and 'javascript' not in nl_lower and 'js' not in nl_lower:
            if 'prime' in nl_lower:
                return "public static boolean isPrime(int n) {"
            else:
                return "public static "
        elif any(lang in nl_lower for lang in ['javascript', 'js']):
            if 'prime' in nl_lower:
                return "function isPrime(n) {"
            else:
                return "function "
        
        # Default fallback
        if 'prime' in nl_lower and 'function' in nl_lower:
            return "def is_prime(n):"
        elif 'function' in nl_lower:
            return "def "
        elif 'class' in nl_lower:
            return "class "
        else:
            return f"// {nl_prompt}\n"


def create_argument_parser(description: str = "VLLM Code Generation") -> object:
    """
    Create common argument parser for VLLM scripts.
    
    Args:
        description: Description for the script
        
    Returns:
        Configured argument parser
    """
    import argparse
    
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("--model", default="codellama/CodeLlama-7b-Python-hf", 
                       help="Model name or path")
    parser.add_argument("--max-tokens", type=int, default=512, 
                       help="Max tokens to generate")
    parser.add_argument("--temperature", type=float, default=0.1, 
                       help="Temperature (0.0 = deterministic)")
    parser.add_argument("--top-p", type=float, default=0.95, 
                       help="Top-p sampling")
    parser.add_argument("--tensor-parallel", type=int, default=1, 
                       help="Tensor parallel size")
    
    return parser


# Default model configurations
DEFAULT_MODELS = {
    "codellama-7b": "codellama/CodeLlama-7b-Python-hf",
    "codellama-13b": "codellama/CodeLlama-13b-Python-hf",
    "wizardcoder": "WizardLM/WizardCoder-Python-7B-V1.0",
    "deepseek": "deepseek-ai/deepseek-coder-6.7b-instruct"
}


# Common stop tokens for different languages
STOP_TOKENS = {
    "python": ["\n\n", "def ", "class ", "import ", "from "],
    "cpp": ["\n\n", "int ", "void ", "class ", "#include"],
    "java": ["\n\n", "public ", "private ", "class ", "import"],
    "javascript": ["\n\n", "function ", "class ", "const ", "let "],
    "general": ["\n\n\n", "```"]
}