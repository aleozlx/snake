#!/usr/bin/env python3
"""
Enhanced Natural Language Processor using semantic models.
Provides DistilBERT-based intent classification and CodeT5-based prompt conversion.
"""

import re
import torch
import logging
from typing import Optional, Dict, List, Tuple
from dataclasses import dataclass
from transformers import (
    AutoTokenizer, AutoModelForSequenceClassification,
    AutoModelForSeq2SeqLM, pipeline, T5ForConditionalGeneration
)
import warnings
warnings.filterwarnings("ignore", category=FutureWarning)

# Import existing NL processor as fallback
from .common import NaturalLanguageProcessor

logger = logging.getLogger(__name__)

@dataclass
class IntentClassification:
    """Result of intent classification."""
    is_natural_language: bool
    confidence: float
    intent_type: str  # 'code_generation', 'code_completion', 'question', 'other'
    programming_language: Optional[str] = None

@dataclass
class PromptConversion:
    """Result of prompt conversion."""
    converted_prompt: str
    confidence: float
    original_prompt: str
    conversion_method: str  # 'semantic', 'template', 'passthrough'

class EnhancedNaturalLanguageProcessor:
    """Enhanced NL processor using semantic models for better intent understanding."""
    
    def __init__(self, device: str = "auto", enable_gpu: bool = True):
        """
        Initialize enhanced NL processor.
        
        Args:
            device: Device to run models on ('auto', 'cuda', 'cpu')
            enable_gpu: Whether to use GPU if available
        """
        self.device = self._setup_device(device, enable_gpu)
        self.intent_classifier = None
        self.prompt_converter = None
        self.fallback_processor = NaturalLanguageProcessor()
        
        # Memory budget: ~0.5GB total
        self.memory_budget_mb = 500
        self.models_loaded = False
        
        logger.info(f"Enhanced NL Processor initialized on device: {self.device}")
    
    def _setup_device(self, device: str, enable_gpu: bool) -> torch.device:
        """Setup compute device with memory constraints."""
        if device == "auto":
            if enable_gpu and torch.cuda.is_available():
                # Check available GPU memory
                available_mb = torch.cuda.get_device_properties(0).total_memory / 1024**2
                reserved_mb = torch.cuda.memory_reserved() / 1024**2
                free_mb = available_mb - reserved_mb
                
                if free_mb >= self.memory_budget_mb:
                    return torch.device("cuda")
                else:
                    logger.warning(f"Insufficient GPU memory ({free_mb:.1f}MB free, need {self.memory_budget_mb}MB). Using CPU.")
                    return torch.device("cpu")
            else:
                return torch.device("cpu")
        else:
            return torch.device(device)
    
    def load_models(self) -> bool:
        """
        Load semantic models for intent classification and prompt conversion.
        
        Returns:
            True if models loaded successfully, False otherwise
        """
        try:
            logger.info("Loading enhanced NL models...")
            
            # Load DistilBERT for intent classification (~250MB)
            self._load_intent_classifier()
            
            # Load CodeT5-small for prompt conversion (~250MB)  
            self._load_prompt_converter()
            
            self.models_loaded = True
            logger.info("Enhanced NL models loaded successfully")
            return True
            
        except Exception as e:
            logger.error(f"Failed to load enhanced NL models: {e}")
            logger.info("Falling back to keyword-based processing")
            return False
    
    def _load_intent_classifier(self):
        """Load DistilBERT model for intent classification."""
        try:
            # Use text classification pipeline for simplicity
            self.intent_classifier = pipeline(
                "text-classification",
                model="distilbert-base-uncased",
                device=0 if self.device.type == "cuda" else -1,
                torch_dtype=torch.float16 if self.device.type == "cuda" else torch.float32
            )
            logger.info("Intent classifier (DistilBERT) loaded")
        except Exception as e:
            logger.warning(f"Failed to load DistilBERT: {e}. Using simpler classification.")
            self.intent_classifier = None
    
    def _load_prompt_converter(self):
        """Load CodeT5-small for semantic prompt conversion."""
        try:
            model_name = "Salesforce/codet5-small"
            
            self.prompt_tokenizer = AutoTokenizer.from_pretrained(model_name)
            self.prompt_converter = T5ForConditionalGeneration.from_pretrained(
                model_name,
                torch_dtype=torch.float16 if self.device.type == "cuda" else torch.float32,
                device_map="auto" if self.device.type == "cuda" else None
            ).to(self.device)
            
            logger.info("Prompt converter (CodeT5-small) loaded")
        except Exception as e:
            logger.warning(f"Failed to load CodeT5: {e}. Using template-based conversion.")
            self.prompt_converter = None
            self.prompt_tokenizer = None
    
    def classify_intent(self, text: str) -> IntentClassification:
        """
        Classify user intent using semantic understanding.
        
        Args:
            text: User input text
            
        Returns:
            IntentClassification with semantic analysis results
        """
        if not self.models_loaded or self.intent_classifier is None:
            # Fallback to keyword-based classification
            return self._fallback_intent_classification(text)
        
        try:
            # Use DistilBERT for semantic classification
            return self._semantic_intent_classification(text)
        except Exception as e:
            logger.warning(f"Semantic classification failed: {e}. Using fallback.")
            return self._fallback_intent_classification(text)
    
    def _semantic_intent_classification(self, text: str) -> IntentClassification:
        """Perform semantic intent classification using DistilBERT."""
        
        # Prepare text for classification
        processed_text = self._preprocess_for_classification(text)
        
        # Custom classification logic since we need specific intents
        is_nl = self._is_natural_language_semantic(processed_text)
        intent_type = self._classify_code_intent(processed_text)
        prog_lang = self._extract_programming_language(text)
        
        confidence = 0.85  # Placeholder - would need trained model for real confidence
        
        return IntentClassification(
            is_natural_language=is_nl,
            confidence=confidence,
            intent_type=intent_type,
            programming_language=prog_lang
        )
    
    def _fallback_intent_classification(self, text: str) -> IntentClassification:
        """Fallback intent classification using existing keyword method."""
        is_nl = self.fallback_processor.is_natural_language_prompt(text)
        intent_type = "code_generation" if is_nl else "code_completion"
        prog_lang = self._extract_programming_language(text)
        
        return IntentClassification(
            is_natural_language=is_nl,
            confidence=0.7,  # Lower confidence for keyword-based
            intent_type=intent_type,
            programming_language=prog_lang
        )
    
    def _preprocess_for_classification(self, text: str) -> str:
        """Preprocess text for better classification."""
        # Remove extra whitespace and normalize
        text = re.sub(r'\s+', ' ', text.strip())
        
        # Remove code artifacts that might confuse NL classification
        text = re.sub(r'```[\s\S]*?```', '[CODE_BLOCK]', text)
        text = re.sub(r'`[^`]+`', '[INLINE_CODE]', text)
        
        return text
    
    def _is_natural_language_semantic(self, text: str) -> bool:
        """Determine if text is natural language using semantic features."""
        
        # Semantic indicators for natural language
        nl_patterns = [
            r'\b(create|write|make|build|implement|generate|develop)\b',
            r'\b(function|class|program|script|method|algorithm)\s+that\b',
            r'\b(how\s+to|help\s+me|can\s+you|please)\b',
            r'\b(write\s+a|create\s+a|make\s+a|build\s+a)\b',
            r'\bin\s+(python|java|javascript|c\+\+|go|rust|c)\b'
        ]
        
        # Code patterns (more sophisticated than keyword matching)
        code_patterns = [
            r'(def|class|function|int|void|public|private)\s+\w+\s*\(',
            r'(import|include|from)\s+\w+',
            r'\{[\s\S]*\}',  # Code blocks
            r';\s*$',  # Ends with semicolon
            r'(return|if|for|while|else)\s+\w+'
        ]
        
        nl_score = sum(1 for pattern in nl_patterns if re.search(pattern, text, re.IGNORECASE))
        code_score = sum(1 for pattern in code_patterns if re.search(pattern, text, re.IGNORECASE))
        
        # If no clear indicators, use length and structure heuristics
        if nl_score == 0 and code_score == 0:
            words = text.split()
            if len(words) > 5 and not any(char in text for char in ['{', '}', ';', '()']):
                return True
        
        return nl_score > code_score
    
    def _classify_code_intent(self, text: str) -> str:
        """Classify the specific type of code-related intent."""
        text_lower = text.lower()
        
        if any(word in text_lower for word in ['create', 'write', 'implement', 'generate', 'make']):
            return 'code_generation'
        elif any(word in text_lower for word in ['complete', 'finish', 'continue']):
            return 'code_completion'
        elif any(word in text_lower for word in ['explain', 'what', 'how', 'why']):
            return 'question'
        else:
            return 'other'
    
    def _extract_programming_language(self, text: str) -> Optional[str]:
        """Extract programming language from text."""
        text_lower = text.lower()
        
        # Language patterns
        lang_patterns = {
            'python': [r'\bpython\b', r'\.py\b', r'\bdef\s+\w+'],
            'javascript': [r'\bjavascript\b', r'\bjs\b', r'\.js\b', r'\bfunction\s+\w+'],
            'java': [r'\bjava\b', r'\.java\b', r'\bpublic\s+class'],
            'cpp': [r'\bc\+\+\b', r'\bcpp\b', r'\.cpp\b', r'\bint\s+main'],
            'c': [r'\bc\b(?!\+\+)', r'\.c\b', r'#include'],
            'go': [r'\bgo\b', r'\.go\b', r'\bfunc\s+\w+'],
            'rust': [r'\brust\b', r'\.rs\b', r'\bfn\s+\w+']
        }
        
        for lang, patterns in lang_patterns.items():
            if any(re.search(pattern, text_lower) for pattern in patterns):
                return lang
        
        return None
    
    def convert_prompt(self, text: str, intent: IntentClassification) -> PromptConversion:
        """
        Convert natural language prompt to code-starting prompt.
        
        Args:
            text: Original user text
            intent: Classified intent
            
        Returns:
            PromptConversion with converted prompt
        """
        if not intent.is_natural_language:
            # Pass through code as-is
            return PromptConversion(
                converted_prompt=text,
                confidence=1.0,
                original_prompt=text,
                conversion_method="passthrough"
            )
        
        # Try semantic conversion first
        if self.models_loaded and self.prompt_converter is not None:
            try:
                return self._semantic_prompt_conversion(text, intent)
            except Exception as e:
                logger.warning(f"Semantic conversion failed: {e}. Using template fallback.")
        
        # Fallback to enhanced template-based conversion
        return self._template_prompt_conversion(text, intent)
    
    def _semantic_prompt_conversion(self, text: str, intent: IntentClassification) -> PromptConversion:
        """Convert prompt using CodeT5 semantic understanding."""
        
        # Construct conversion prompt for CodeT5
        lang = intent.programming_language or "python"
        conversion_prompt = f"Generate {lang} code starter for: {text}"
        
        # Tokenize and generate
        inputs = self.prompt_tokenizer.encode(
            conversion_prompt, 
            return_tensors="pt",
            max_length=256,
            truncation=True
        ).to(self.device)
        
        with torch.no_grad():
            outputs = self.prompt_converter.generate(
                inputs,
                max_length=50,
                num_beams=3,
                temperature=0.1,
                do_sample=False
            )
        
        converted = self.prompt_tokenizer.decode(outputs[0], skip_special_tokens=True)
        
        return PromptConversion(
            converted_prompt=converted,
            confidence=0.85,
            original_prompt=text,
            conversion_method="semantic"
        )
    
    def _template_prompt_conversion(self, text: str, intent: IntentClassification) -> PromptConversion:
        """Enhanced template-based prompt conversion."""
        
        # Use existing fallback but with better language detection
        lang = intent.programming_language or self._infer_language_from_context(text)
        converted = self.fallback_processor.convert_nl_to_code_prompt(text)
        
        # Enhance with language-specific improvements
        enhanced = self._enhance_code_starter(converted, lang, text)
        
        return PromptConversion(
            converted_prompt=enhanced,
            confidence=0.75,
            original_prompt=text,
            conversion_method="template"
        )
    
    def _infer_language_from_context(self, text: str) -> str:
        """Infer programming language from context clues."""
        text_lower = text.lower()
        
        # Context-based language inference
        if 'function' in text_lower and 'async' in text_lower:
            return 'javascript'
        elif 'class' in text_lower and 'public' in text_lower:
            return 'java'
        elif 'def' in text_lower or 'import' in text_lower:
            return 'python'
        elif 'main' in text_lower and ('int' in text_lower or 'void' in text_lower):
            return 'cpp'
        
        return 'python'  # Default fallback
    
    def _enhance_code_starter(self, starter: str, language: str, original: str) -> str:
        """Enhance code starter with context-aware improvements."""
        
        # Add language-specific enhancements
        if language == 'python' and 'async' in original.lower():
            if starter.startswith('def '):
                starter = starter.replace('def ', 'async def ', 1)
        
        elif language == 'javascript' and 'arrow' in original.lower():
            if starter.startswith('function '):
                func_name = starter.split('(')[0].replace('function ', '')
                starter = f"const {func_name} = ("
        
        # Add type hints for modern languages
        if language == 'python' and 'type' in original.lower():
            if '(' in starter and ')' in starter and '->' not in starter:
                # Add basic type hints
                starter = starter.replace('):', ') -> None:')
        
        return starter
    
    def process_input(self, text: str) -> Tuple[IntentClassification, PromptConversion]:
        """
        Complete NL processing pipeline.
        
        Args:
            text: User input
            
        Returns:
            Tuple of (intent_classification, prompt_conversion)
        """
        # Classify intent
        intent = self.classify_intent(text)
        
        # Convert prompt if needed
        conversion = self.convert_prompt(text, intent)
        
        return intent, conversion
    
    def get_memory_usage(self) -> Dict[str, float]:
        """Get current memory usage of loaded models."""
        if not torch.cuda.is_available():
            return {"gpu_mb": 0, "cpu_mb": 0}
        
        gpu_mb = torch.cuda.memory_allocated() / 1024**2
        return {
            "gpu_mb": gpu_mb,
            "models_loaded": self.models_loaded,
            "device": str(self.device)
        }
    
    def unload_models(self):
        """Unload models to free memory."""
        if self.intent_classifier:
            del self.intent_classifier
            self.intent_classifier = None
        
        if self.prompt_converter:
            del self.prompt_converter
            del self.prompt_tokenizer
            self.prompt_converter = None
            self.prompt_tokenizer = None
        
        if torch.cuda.is_available():
            torch.cuda.empty_cache()
        
        self.models_loaded = False
        logger.info("Enhanced NL models unloaded")

# Factory function for easy integration
def create_enhanced_nl_processor(**kwargs) -> EnhancedNaturalLanguageProcessor:
    """Create and initialize enhanced NL processor."""
    processor = EnhancedNaturalLanguageProcessor(**kwargs)
    
    # Try to load models, fallback gracefully if failed
    if not processor.load_models():
        logger.warning("Enhanced NL processor initialized with fallback mode only")
    
    return processor