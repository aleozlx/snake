#!/usr/bin/env python3
"""Universal runner for interactive code generation agents using factory pattern."""

import sys
import argparse
from factory import create_agent


def main():
    parser = argparse.ArgumentParser(description="Interactive Code Generation Agent")
    parser.add_argument("--agent-type", default="a2", choices=["a1", "a2"],
                       help="Agent type (a1=basic, a2=interactive)")
    parser.add_argument("--backend", default="vllm", choices=["vllm", "mock"],
                       help="Backend to use (default: vllm)")
    parser.add_argument("--model", default="codellama/CodeLlama-7b-Python-hf",
                       help="Model name (for VLLM backend)")
    parser.add_argument("--code-completion", action="store_true",
                       help="Disable natural language processing, use basic code completion only (a2 only)")
    parser.add_argument("--preset", default="balanced", 
                       choices=["fast", "creative", "precise", "balanced"],
                       help="Generation preset")
    
    args = parser.parse_args()
    
    try:
        agent_names = {"a1": "basic", "a2": "interactive"}
        mode_suffix = " (code completion)" if args.code_completion and args.agent_type == "a2" else ""
        print(f"🚀 Starting {agent_names[args.agent_type]} agent{mode_suffix} with {args.backend} backend...")
        
        # Create agent using factory
        agent = create_agent(
            agent_type=args.agent_type,
            backend_type=args.backend,
            model_name=args.model if args.backend == "vllm" else None,
            natural_language=not args.code_completion if args.agent_type == "a2" else False
        )
        
        # Start interactive session
        agent.run_interactive_session()
        
    except KeyboardInterrupt:
        print("\nGoodbye!")
    except Exception as e:
        print(f"Error: {e}")
        return 1
    
    return 0


if __name__ == "__main__":
    sys.exit(main())