#!/usr/bin/env python3
"""
VLLM Code Generation Script (Refactored with Dependency Injection)

This script demonstrates batch code generation using VLLM with dependency injection
for improved testability and modularity.

Usage:
    python a1_refactored.py "def fibonacci(n):"
    python a1_refactored.py --backend mock "def test():"

Example:
    python a1_refactored.py --model "codellama/CodeLlama-7b-Python-hf" "def fibonacci(n):"

Requirements:
    pip install vllm

Supported Models:
    - codellama/CodeLlama-7b-Python-hf
    - codellama/CodeLlama-13b-Python-hf
    - WizardLM/WizardCoder-Python-7B-V1.0
    - deepseek-ai/deepseek-coder-6.7b-instruct
"""

import argparse
import os
from typing import List, Optional
from backend import CodeGenerationBackend


class VLLMCodeGenerator:
    def __init__(self, backend: CodeGenerationBackend):
        """
        Initialize VLLM code generator with dependency injection.

        Args:
            backend: Code generation backend (VLLM, Mock, etc.)
        """
        self.backend = backend

    def generate_code(
        self,
        prompts: List[str],
        max_tokens: int = 512,
        temperature: float = 0.1,
        top_p: float = 0.95,
        stop_tokens: Optional[List[str]] = None
    ) -> List[str]:
        """
        Generate code completions for given prompts.

        Args:
            prompts: List of code prompts
            max_tokens: Maximum tokens to generate
            temperature: Sampling temperature (0.0 = deterministic)
            top_p: Top-p sampling parameter
            stop_tokens: List of stop tokens

        Returns:
            List of generated code completions
        """
        return self.backend.batch_generate(
            prompts=prompts,
            max_tokens=max_tokens,
            temperature=temperature,
            top_p=top_p,
            stop_tokens=stop_tokens
        )

    def generate_function(self, function_signature: str, docstring: str = "") -> str:
        """
        Generate a complete function implementation.

        Args:
            function_signature: Function signature (e.g., "def fibonacci(n):")
            docstring: Optional function docstring

        Returns:
            Complete function implementation
        """
        if docstring:
            prompt = f'{function_signature}\\n    """{docstring}"""\\n    '
        else:
            prompt = f'{function_signature}\\n    '

        completions = self.backend.generate(prompt, max_tokens=256)
        return function_signature + "\\n    " + completions


def main():
    parser = argparse.ArgumentParser(description="VLLM Code Generation")
    parser.add_argument("prompt", help="Code prompt")
    parser.add_argument("--model", default="codellama/CodeLlama-7b-Python-hf", help="Model name or path")
    parser.add_argument("--max-tokens", type=int, default=512, help="Max tokens")
    parser.add_argument("--temperature", type=float, default=0.1, help="Temperature")
    parser.add_argument("--top-p", type=float, default=0.95, help="Top-p sampling")
    parser.add_argument("--tensor-parallel", type=int, default=1, help="Tensor parallel size")
    parser.add_argument("--backend", default="vllm", choices=["vllm", "mock"],
                       help="Backend type to use (default: vllm)")

    args = parser.parse_args()

    # Set CUDA device if not set
    if "CUDA_VISIBLE_DEVICES" not in os.environ:
        os.environ["CUDA_VISIBLE_DEVICES"] = "0"

    print(f"Loading backend: {args.backend}")
    if args.backend == "vllm":
        print(f"Model: {args.model}")
        
    try:
        # Use factory to create agent
        from factory import create_agent
        generator = create_agent(
            agent_type="a1",
            backend_type=args.backend,
            model_name=args.model,
            tensor_parallel_size=args.tensor_parallel
        )

        print(f"Generating code for prompt: {args.prompt}")
        completions = generator.generate_code(
            [args.prompt],
            max_tokens=args.max_tokens,
            temperature=args.temperature,
            top_p=args.top_p
        )

        print("\\nGenerated Code:")
        print("-" * 50)
        print(args.prompt + completions[0])
        print("-" * 50)
        
    except Exception as e:
        print(f"Error: {e}")
        return 1

    return 0


if __name__ == "__main__":
    exit(main())