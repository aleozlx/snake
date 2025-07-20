#!/usr/bin/env python3
"""
Interactive VLLM Code Generation Agent (Refactored with Dependency Injection)

This script provides an interactive interface for iterative code generation.
It allows users to provide initial prompts and then follow up with refinements
until they are satisfied with the generated code.

Features:
- Interactive prompt input
- Iterative refinement with follow-up prompts  
- Session management with commands
- Optional natural language processing
- Dependency injection for easy testing

Usage:
    from factory import create_agent
    agent = create_agent("a2", backend_type="vllm")
    agent.run_interactive_session()

    # With natural language support
    agent = create_agent("a2", backend_type="vllm", natural_language=True)
    agent.run_interactive_session()

Commands during interaction:
    - Type your prompt and press Enter
    - Type 'done' to exit
    - Type 'new session' to restart
    - Type 'history' to see all previous iterations

Natural Language Examples (when enabled):
    User: "create a prime function in python"
    → def is_prime(n):
    
    User: "write a prime checker in c++"
    → bool isPrime(int n) {
"""

import argparse
import os
import sys
from typing import List, Optional, Dict, Any
from backend import CodeGenerationBackend


class InteractiveCodeGenerator:
    def __init__(self, backend: CodeGenerationBackend, natural_language: bool = True):
        """
        Initialize interactive VLLM code generator with dependency injection.

        Args:
            backend: Code generation backend (VLLM, Mock, etc.)
            natural_language: Enable natural language processing (default: True)
        """
        self.backend = backend
        self.conversation_history = []
        self.current_code = ""
        self.iteration_count = 0
        self.natural_language = natural_language
        
        # Initialize natural language processor if enabled
        if self.natural_language:
            from common import NaturalLanguageProcessor
            self.nl_processor = NaturalLanguageProcessor()
        else:
            self.nl_processor = None

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
        return self.backend.generate(
            prompt=prompt,
            max_tokens=max_tokens,
            temperature=temperature,
            top_p=top_p,
            stop_tokens=stop_tokens
        )

    def is_natural_language_prompt(self, text: str) -> bool:
        """
        Detect if the input is a natural language request vs code.
        Only available when natural_language=True.
        """
        if not self.natural_language or not self.nl_processor:
            return False
        return self.nl_processor.is_natural_language_prompt(text)

    def convert_nl_to_code_prompt(self, nl_prompt: str) -> str:
        """
        Convert natural language prompt to a code-starting prompt.
        Only available when natural_language=True.
        """
        if not self.natural_language or not self.nl_processor:
            return nl_prompt
        return self.nl_processor.convert_nl_to_code_prompt(nl_prompt)

    def build_context_prompt(self, user_input: str) -> str:
        """
        Build a context-aware prompt including conversation history.

        Args:
            user_input: Current user input/request

        Returns:
            Complete prompt with context
        """
        if self.iteration_count == 0:
            # First iteration - check for natural language if enabled
            if self.natural_language and self.is_natural_language_prompt(user_input):
                code_prompt = self.convert_nl_to_code_prompt(user_input)
                print(f"→ {code_prompt}")
                return code_prompt
            else:
                return user_input
        else:
            # Follow-up iteration - include context
            context = f"Current code:\n{self.current_code}\n\n"
            context += f"User request: {user_input}\n\n"
            context += "Improved code:\n"
            return context

    def display_code(self, code: str, iteration: int):
        """Display generated code with formatting."""
        print(f"\n{'='*60}")
        print(f"ITERATION {iteration} - Generated Code:")
        print(f"{'='*60}")
        print(code)
        print(f"{'='*60}")

    def get_input(self, prompt_text: str = "Enter your prompt") -> str:
        """
        Get single line input from user.
        
        Returns:
            User input string
        """
        try:
            return input(f"{prompt_text}: ").strip()
        except KeyboardInterrupt:
            print("\nExiting...")
            sys.exit(0)

    def show_history(self):
        """Display conversation history."""
        print(f"\n{'='*60}")
        print("CONVERSATION HISTORY:")
        print(f"{'='*60}")
        for i, entry in enumerate(self.conversation_history, 1):
            print(f"\nIteration {i}:")
            print(f"User: {entry['user_input']}")
            print(f"Generated Code:\n{entry['generated_code']}")
            print("-" * 40)

    def run_interactive_session(self, max_tokens: int = 512, temperature: float = 0.1, top_p: float = 0.95):
        """
        Run the main interactive session.
        
        Args:
            max_tokens: Maximum tokens to generate
            temperature: Sampling temperature
            top_p: Top-p sampling parameter
        """
        print(f"🤖 Interactive Code Generation Agent")
        print("Commands: 'done' to exit, 'new session' to restart, 'history' to see history")

        while True:
            try:
                if self.iteration_count == 0:
                    user_input = self.get_input("Initial prompt")
                else:
                    user_input = self.get_input("Follow-up")

                # Handle special commands
                if user_input.lower() in ['done', 'quit', 'exit']:
                    if self.current_code:
                        print(f"\nFinal code:\n{'-'*40}")
                        print(self.current_code)
                        print(f"{'-'*40}")
                    print("Goodbye!")
                    break
                elif user_input.lower() == 'new session':
                    self.conversation_history = []
                    self.current_code = ""
                    self.iteration_count = 0
                    print("New session started!")
                    continue
                elif user_input.lower() == 'history':
                    self.show_history()
                    continue
                elif not user_input.strip():
                    print("Please enter a prompt or command.")
                    continue

                # Generate code
                self.iteration_count += 1
                print(f"\n🔄 Processing iteration {self.iteration_count}...")
                
                context_prompt = self.build_context_prompt(user_input)
                generated_code = self.generate_code(
                    context_prompt,
                    max_tokens=max_tokens,
                    temperature=temperature,
                    top_p=top_p
                )

                # Clean up the generated code
                if generated_code.startswith('```'):
                    # Remove markdown code blocks if present
                    lines = generated_code.split('\n')
                    if lines[0].startswith('```'):
                        lines = lines[1:]
                    if lines and lines[-1].startswith('```'):
                        lines = lines[:-1]
                    generated_code = '\n'.join(lines)

                # Update current code - use only the generated code, not concatenated with input
                self.current_code = generated_code.strip()

                # Display the generated code
                self.display_code(self.current_code, self.iteration_count)

                # Store in history
                self.conversation_history.append({
                    'user_input': user_input,
                    'generated_code': self.current_code,
                    'iteration': self.iteration_count
                })
                    
            except KeyboardInterrupt:
                print("\\n\\nExiting...")
                break
            except Exception as e:
                print(f"\\nError: {e}")
                print("Please try again.")


def main():
    parser = argparse.ArgumentParser(description="Interactive VLLM Code Generation")
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
    parser.add_argument("--backend", default="vllm", choices=["vllm", "mock"],
                       help="Backend type to use (default: vllm)")

    args = parser.parse_args()

    # Set CUDA device if not set
    if "CUDA_VISIBLE_DEVICES" not in os.environ:
        os.environ["CUDA_VISIBLE_DEVICES"] = "0"

    print(f"🚀 Loading backend: {args.backend}")
    if args.backend == "vllm":
        print(f"Model: {args.model}")
        print("This may take a moment...")
    
    try:
        # Use factory to create agent
        from factory import create_agent
        agent = create_agent(
            agent_type="a2",
            backend_type=args.backend,
            model_name=args.model,
            tensor_parallel_size=args.tensor_parallel
        )
        
        agent.run_interactive_session(
            max_tokens=args.max_tokens,
            temperature=args.temperature,
            top_p=args.top_p
        )
    except KeyboardInterrupt:
        print("\\nExiting...")
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()