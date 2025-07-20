#!/usr/bin/env python3
"""
Memory benchmarking tool for RTX 3090 model loading.
Measures GPU memory usage for different models and quantization options.
"""

import gc
import time
import torch
import psutil
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass
import subprocess
import sys

@dataclass
class MemoryStats:
    """Memory usage statistics."""
    gpu_allocated_mb: float
    gpu_reserved_mb: float
    gpu_free_mb: float
    ram_used_mb: float
    model_name: str
    quantization: Optional[str] = None
    loading_time_seconds: float = 0.0

class MemoryBenchmark:
    """Benchmarks memory usage for different model configurations."""
    
    def __init__(self):
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        self.gpu_total_mb = torch.cuda.get_device_properties(0).total_memory / 1024**2 if torch.cuda.is_available() else 0
        
    def get_gpu_memory_stats(self) -> Tuple[float, float, float]:
        """Get current GPU memory usage in MB."""
        if not torch.cuda.is_available():
            return 0.0, 0.0, 0.0
            
        allocated = torch.cuda.memory_allocated() / 1024**2
        reserved = torch.cuda.memory_reserved() / 1024**2
        free = self.gpu_total_mb - reserved
        return allocated, reserved, free
    
    def get_ram_usage(self) -> float:
        """Get current RAM usage in MB."""
        return psutil.Process().memory_info().rss / 1024**2
    
    def clear_memory(self):
        """Clear GPU and RAM caches."""
        if torch.cuda.is_available():
            torch.cuda.empty_cache()
        gc.collect()
        time.sleep(1)  # Give system time to clean up
    
    def benchmark_vllm_model(self, model_name: str, quantization: Optional[str] = None) -> MemoryStats:
        """Benchmark memory usage for a VLLM model."""
        self.clear_memory()
        
        # Get baseline memory
        baseline_gpu = self.get_gpu_memory_stats()
        baseline_ram = self.get_ram_usage()
        
        start_time = time.time()
        
        try:
            # Import here to avoid issues if vllm not available
            from vllm import LLM
            
            # Configure model parameters
            kwargs = {
                "model": model_name,
                "gpu_memory_utilization": 0.9,
                "max_model_len": 8192,
                "trust_remote_code": True,
                "tensor_parallel_size": 1,
            }
            
            # Add quantization if specified
            if quantization:
                if quantization == "4bit":
                    kwargs["quantization"] = "bitsandbytes"
                    kwargs["load_in_4bit"] = True
                elif quantization == "8bit":
                    kwargs["quantization"] = "bitsandbytes" 
                    kwargs["load_in_8bit"] = True
            
            # Load model
            llm = LLM(**kwargs)
            loading_time = time.time() - start_time
            
            # Measure memory after loading
            gpu_allocated, gpu_reserved, gpu_free = self.get_gpu_memory_stats()
            ram_used = self.get_ram_usage()
            
            # Clean up
            del llm
            self.clear_memory()
            
            return MemoryStats(
                gpu_allocated_mb=gpu_allocated,
                gpu_reserved_mb=gpu_reserved,
                gpu_free_mb=gpu_free,
                ram_used_mb=ram_used - baseline_ram,
                model_name=model_name,
                quantization=quantization,
                loading_time_seconds=loading_time
            )
            
        except Exception as e:
            print(f"Error loading {model_name}: {e}")
            return MemoryStats(
                gpu_allocated_mb=0,
                gpu_reserved_mb=0,
                gpu_free_mb=self.gpu_total_mb,
                ram_used_mb=0,
                model_name=model_name,
                quantization=quantization,
                loading_time_seconds=0
            )
    
    def benchmark_transformers_model(self, model_name: str, quantization: Optional[str] = None) -> MemoryStats:
        """Benchmark memory usage for a transformers model."""
        self.clear_memory()
        
        baseline_ram = self.get_ram_usage()
        start_time = time.time()
        
        try:
            from transformers import AutoTokenizer, AutoModelForCausalLM
            import torch
            
            # Configure model loading
            kwargs = {"trust_remote_code": True}
            
            if quantization == "4bit":
                from transformers import BitsAndBytesConfig
                kwargs["quantization_config"] = BitsAndBytesConfig(load_in_4bit=True)
            elif quantization == "8bit":
                from transformers import BitsAndBytesConfig  
                kwargs["quantization_config"] = BitsAndBytesConfig(load_in_8bit=True)
            else:
                kwargs["torch_dtype"] = torch.float16
                kwargs["device_map"] = "auto"
            
            # Load model
            tokenizer = AutoTokenizer.from_pretrained(model_name)
            model = AutoModelForCausalLM.from_pretrained(model_name, **kwargs)
            
            loading_time = time.time() - start_time
            
            # Measure memory
            gpu_allocated, gpu_reserved, gpu_free = self.get_gpu_memory_stats()
            ram_used = self.get_ram_usage()
            
            # Clean up
            del model, tokenizer
            self.clear_memory()
            
            return MemoryStats(
                gpu_allocated_mb=gpu_allocated,
                gpu_reserved_mb=gpu_reserved,
                gpu_free_mb=gpu_free,
                ram_used_mb=ram_used - baseline_ram,
                model_name=model_name,
                quantization=quantization,
                loading_time_seconds=loading_time
            )
            
        except Exception as e:
            print(f"Error loading {model_name}: {e}")
            return MemoryStats(
                gpu_allocated_mb=0,
                gpu_reserved_mb=0,
                gpu_free_mb=self.gpu_total_mb,
                ram_used_mb=0,
                model_name=model_name,
                quantization=quantization,
                loading_time_seconds=0
            )

def run_benchmark():
    """Run comprehensive memory benchmark."""
    benchmark = MemoryBenchmark()
    
    print(f"GPU Total Memory: {benchmark.gpu_total_mb:.1f} MB")
    print(f"Device: {benchmark.device}")
    print("="*80)
    
    # Models to test
    code_models = [
        "codellama/CodeLlama-7b-Python-hf",
        "codellama/CodeLlama-13b-Python-hf", 
        "WizardLM/WizardCoder-Python-7B-V1.0",
        "deepseek-ai/deepseek-coder-6.7b-instruct"
    ]
    
    nl_models = [
        "microsoft/codebert-base",
        "Salesforce/codet5-small",
        "Salesforce/codet5-base",
        "microsoft/DialoGPT-small",
        "distilbert-base-uncased"
    ]
    
    quantizations = [None, "4bit", "8bit"]
    
    results = []
    
    print("Testing Code Models (VLLM):")
    print("-" * 40)
    for model in code_models:
        for quant in quantizations:
            print(f"Testing {model} ({quant or 'fp16'})...")
            stats = benchmark.benchmark_vllm_model(model, quant)
            results.append(stats)
            print(f"  GPU Reserved: {stats.gpu_reserved_mb:.1f}MB, "
                  f"Free: {stats.gpu_free_mb:.1f}MB, "
                  f"Load Time: {stats.loading_time_seconds:.1f}s")
    
    print("\nTesting NL Models (Transformers):")
    print("-" * 40)
    for model in nl_models:
        for quant in quantizations:
            print(f"Testing {model} ({quant or 'fp16'})...")
            stats = benchmark.benchmark_transformers_model(model, quant)
            results.append(stats)
            print(f"  GPU Reserved: {stats.gpu_reserved_mb:.1f}MB, "
                  f"Free: {stats.gpu_free_mb:.1f}MB, "
                  f"Load Time: {stats.loading_time_seconds:.1f}s")
    
    # Generate report
    print("\n" + "="*80)
    print("MEMORY BENCHMARK REPORT")
    print("="*80)
    
    # Best options for RTX 3090
    viable_combinations = []
    for stats in results:
        if stats.gpu_reserved_mb > 0 and stats.gpu_free_mb > 2000:  # Leave 2GB free
            viable_combinations.append(stats)
    
    print(f"\nViable Model Combinations (leaving >2GB free):")
    print("-" * 50)
    for stats in sorted(viable_combinations, key=lambda x: x.gpu_reserved_mb):
        print(f"{stats.model_name} ({stats.quantization or 'fp16'})")
        print(f"  Memory: {stats.gpu_reserved_mb:.1f}MB, Free: {stats.gpu_free_mb:.1f}MB")
        print(f"  Load Time: {stats.loading_time_seconds:.1f}s")
        print()
    
    # Dual model scenarios
    print("\nDual Model Scenarios:")
    print("-" * 30)
    code_stats = [s for s in results if any(model in s.model_name for model in code_models)]
    nl_stats = [s for s in results if any(model in s.model_name for model in nl_models)]
    
    for code_stat in code_stats:
        if code_stat.gpu_reserved_mb == 0:
            continue
        for nl_stat in nl_stats:
            if nl_stat.gpu_reserved_mb == 0:
                continue
            total_memory = code_stat.gpu_reserved_mb + nl_stat.gpu_reserved_mb
            if total_memory < benchmark.gpu_total_mb - 2000:  # Leave 2GB free
                print(f"✓ {code_stat.model_name} ({code_stat.quantization or 'fp16'}) + "
                      f"{nl_stat.model_name} ({nl_stat.quantization or 'fp16'})")
                print(f"  Total: {total_memory:.1f}MB, Free: {benchmark.gpu_total_mb - total_memory:.1f}MB")

if __name__ == "__main__":
    run_benchmark()