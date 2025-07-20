#!/usr/bin/env python3
"""
VLLM Code Generation Script

This script demonstrates how to use VLLM for code generation tasks.
VLLM is a fast and memory-efficient inference engine for large language models.

Usage:
    python vllm_codegen.py --model <model_name> --prompt <code_prompt>

Example:
    python vllm_codegen.py --model "codellama/CodeLlama-7b-Python-hf" --prompt "def fibonacci(n):"

Requirements:
    pip install vllm

Supported Models:
    - codellama/CodeLlama-7b-Python-hf
    - codellama/CodeLlama-13b-Python-hf
    - WizardLM/WizardCoder-Python-7B-V1.0
    - deepseek-ai/deepseek-coder-6.7b-instruct

Environment Variables:
    CUDA_VISIBLE_DEVICES: Set GPU devices (default: "0")
    VLLM_TENSOR_PARALLEL_SIZE: Number of GPUs for tensor parallelism (default: 1)
"""

import argparse
import os
from typing import List, Optional
from vllm import LLM, SamplingParams


class VLLMCodeGenerator:
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
            prompt = f'{function_signature}\n    """{docstring}"""\n    '
        else:
            prompt = f'{function_signature}\n    '

        completions = self.generate_code([prompt], max_tokens=256)
        return function_signature + "\n    " + completions[0]


def main():
    parser = argparse.ArgumentParser(description="VLLM Code Generation")
    parser.add_argument("prompt", help="Code prompt")
    parser.add_argument("--model", default="codellama/CodeLlama-7b-Python-hf", help="Model name or path")
    parser.add_argument("--max-tokens", type=int, default=512, help="Max tokens")
    parser.add_argument("--temperature", type=float, default=0.1, help="Temperature")
    parser.add_argument("--top-p", type=float, default=0.95, help="Top-p sampling")
    parser.add_argument("--tensor-parallel", type=int, default=1, help="Tensor parallel size")

    args = parser.parse_args()

    # Set CUDA device if not set
    if "CUDA_VISIBLE_DEVICES" not in os.environ:
        os.environ["CUDA_VISIBLE_DEVICES"] = "0"

    print(f"Loading model: {args.model}")
    generator = VLLMCodeGenerator(args.model, args.tensor_parallel)

    print(f"Generating code for prompt: {args.prompt}")
    completions = generator.generate_code(
        [args.prompt],
        max_tokens=args.max_tokens,
        temperature=args.temperature,
        top_p=args.top_p
    )

    print("\nGenerated Code:")
    print("-" * 50)
    print(args.prompt + completions[0])
    print("-" * 50)


if __name__ == "__main__":
    main()
