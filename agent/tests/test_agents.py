"""Simple tests using dependency injection - no complex mocking needed!"""

import pytest
import sys
from pathlib import Path

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))

from backend import MockBackend
from factory import create_agent, create_mock_agent


class TestBackends:
    """Test backend functionality."""
    
    @pytest.mark.smoke
    def test_mock_backend_basic(self):
        """Test basic mock backend functionality."""
        backend = MockBackend()
        
        # Test single generation
        result = backend.generate("def fibonacci")
        assert "def fibonacci" in result
        assert "return" in result
        
        # Test batch generation
        results = backend.batch_generate(["def test1", "def test2"])
        assert len(results) == 2
        assert all(len(r) > 0 for r in results)
    
    @pytest.mark.smoke
    def test_mock_backend_custom_responses(self):
        """Test mock backend with custom responses."""
        custom_responses = {
            "def hello": "def hello():\n    print('Hello, World!')",
            "class Calculator": "class Calculator:\n    def add(self, a, b):\n        return a + b"
        }
        
        backend = MockBackend(custom_responses)
        
        result = backend.generate("def hello")
        assert "Hello, World!" in result
        
        result = backend.generate("class Calculator")
        assert "def add" in result
    
    @pytest.mark.smoke
    def test_mock_backend_generation_count(self):
        """Test that mock backend tracks generation count."""
        backend = MockBackend()
        
        initial_count = backend.generation_count
        backend.generate("def test")
        assert backend.generation_count == initial_count + 1
        
        backend.batch_generate(["def test1", "def test2"])
        assert backend.generation_count == initial_count + 3


class TestAgentCreation:
    """Test agent creation using factory pattern."""
    
    @pytest.mark.smoke
    def test_create_mock_agent_a1(self):
        """Test creating a1 agent with mock backend."""
        agent = create_mock_agent("a1")
        
        assert hasattr(agent, 'backend')
        assert hasattr(agent, 'generate_code')
        # a1 uses batch generation
        result = agent.generate_code(["def test"])
        assert len(result) == 1
        assert len(result[0]) > 0
    
    @pytest.mark.smoke
    def test_create_mock_agent_a2(self):
        """Test creating a2 agent with mock backend (natural language default)."""
        agent = create_mock_agent("a2")
        
        assert hasattr(agent, 'backend')
        assert hasattr(agent, 'generate_code')
        assert hasattr(agent, 'conversation_history')
        assert agent.iteration_count == 0
        assert agent.natural_language == True  # Now defaults to True
    
    @pytest.mark.smoke
    def test_create_mock_agent_a2_code_completion(self):
        """Test creating a2 agent with code completion mode (no natural language)."""
        agent = create_mock_agent("a2", natural_language=False)
        
        assert hasattr(agent, 'backend')
        assert hasattr(agent, 'generate_code')
        assert hasattr(agent, 'is_natural_language_prompt')
        assert hasattr(agent, 'convert_nl_to_code_prompt')
        assert agent.natural_language == False
    
    @pytest.mark.smoke
    def test_create_agent_with_factory(self):
        """Test creating agent using main factory function."""
        agent = create_agent("a2", backend_type="mock")
        
        assert hasattr(agent, 'backend')
        assert hasattr(agent, 'generate_code')
        
        # Test that it can generate code
        result = agent.generate_code("def test")
        assert len(result) > 0


class TestAgentFunctionality:
    """Test agent functionality with mock backends."""
    
    @pytest.mark.smoke
    def test_a1_batch_generation(self):
        """Test a1 agent batch code generation."""
        agent = create_mock_agent("a1")
        
        prompts = ["def fibonacci", "def factorial"]
        results = agent.generate_code(prompts)
        assert len(results) == 2
        assert all(len(r) > 0 for r in results)
        assert "fibonacci" in results[0]
    
    @pytest.mark.smoke
    def test_a2_code_generation(self):
        """Test a2 agent code generation."""
        agent = create_mock_agent("a2")
        
        result = agent.generate_code("def fibonacci")
        assert "fibonacci" in result
        assert len(result) > 0
    
    @pytest.mark.smoke
    def test_a2_context_building(self):
        """Test a2 agent context building."""
        agent = create_mock_agent("a2")
        
        # First iteration should return input directly
        context = agent.build_context_prompt("def test")
        assert context == "def test"
        
        # Simulate iteration
        agent.iteration_count = 1
        agent.current_code = "def test():\n    pass"
        
        context = agent.build_context_prompt("add docstring")
        assert "Current code:" in context
        assert "def test" in context
        assert "add docstring" in context
    
    @pytest.mark.smoke
    def test_a2_natural_language_features(self):
        """Test a2 agent with natural language features (default behavior)."""
        agent = create_mock_agent("a2")  # Natural language is now default
        
        # Test detection
        assert agent.is_natural_language_prompt("create a function") == True
        assert agent.is_natural_language_prompt("def test():") == False
        
        # Test conversion
        result = agent.convert_nl_to_code_prompt("create a prime function in python")
        assert "def is_prime" in result
    
    @pytest.mark.smoke
    def test_a2_context_with_nl_conversion(self):
        """Test a2 agent context building with natural language conversion (default)."""
        agent = create_mock_agent("a2")  # Natural language is now default
        
        # Should convert natural language on first iteration
        context = agent.build_context_prompt("create a prime function in python")
        assert "def is_prime" in context
    
    @pytest.mark.smoke
    def test_a2_code_completion_mode(self):
        """Test a2 agent in code completion mode (no natural language)."""
        agent = create_mock_agent("a2", natural_language=False)
        
        # Should not have NL processing
        assert agent.is_natural_language_prompt("create a function") == False
        assert agent.convert_nl_to_code_prompt("create a function") == "create a function"


class TestAgentInteraction:
    """Test agent interaction patterns."""
    
    @pytest.mark.smoke
    def test_agent_history_management(self):
        """Test conversation history management."""
        agent = create_mock_agent("a2")
        
        # Initially empty
        assert len(agent.conversation_history) == 0
        
        # Add history entry
        agent.conversation_history.append({
            'user_input': 'def test',
            'generated_code': 'def test():\n    pass',
            'iteration': 1
        })
        
        assert len(agent.conversation_history) == 1
        assert agent.conversation_history[0]['iteration'] == 1
    
    @pytest.mark.smoke
    def test_agent_session_reset(self):
        """Test agent session reset functionality."""
        agent = create_mock_agent("a2")
        
        # Set some state
        agent.iteration_count = 5
        agent.current_code = "some code"
        agent.conversation_history = [{"test": "data"}]
        
        # Reset state (simulating 'new session' command)
        agent.iteration_count = 0
        agent.current_code = ""
        agent.conversation_history = []
        
        # Verify reset
        assert agent.iteration_count == 0
        assert agent.current_code == ""
        assert len(agent.conversation_history) == 0


class TestIntegrationWithMocks:
    """Test integration scenarios using mock backends."""
    
    @pytest.mark.smoke
    def test_complete_interaction_simulation(self):
        """Test complete interaction simulation."""
        # Custom responses for realistic interaction
        responses = {
            "def fibonacci": """def fibonacci(n):
    if n <= 1:
        return n
    return fibonacci(n-1) + fibonacci(n-2)""",
            "add docstring": """def fibonacci(n):
    \"\"\"Calculate Fibonacci number.\"\"\"
    if n <= 1:
        return n
    return fibonacci(n-1) + fibonacci(n-2)"""
        }
        
        agent = create_mock_agent("a2", responses=responses)
        
        # Simulate first iteration
        result1 = agent.generate_code("def fibonacci")
        assert "fibonacci" in result1
        assert "return" in result1
        
        # Simulate context building for second iteration
        agent.iteration_count = 1
        agent.current_code = result1
        
        context = agent.build_context_prompt("add docstring")
        assert "Current code:" in context
        
        # Simulate second iteration
        result2 = agent.generate_code(context)
        assert len(result2) > 0
    
    @pytest.mark.smoke
    def test_natural_language_workflow(self):
        """Test natural language workflow with a2 agent (default behavior)."""
        agent = create_mock_agent("a2")  # Natural language is now default
        
        # Test natural language detection and conversion
        nl_input = "create a prime function in python"
        assert agent.is_natural_language_prompt(nl_input)
        
        converted = agent.convert_nl_to_code_prompt(nl_input)
        assert "def is_prime" in converted
        
        # Test code generation with converted prompt
        result = agent.generate_code(converted)
        assert "def is_prime" in result
    
    @pytest.mark.smoke
    def test_error_handling(self):
        """Test error handling in agents."""
        agent = create_mock_agent("a2")
        
        # Test with empty prompt
        result = agent.generate_code("")
        assert isinstance(result, str)  # Should return something, not crash
        
        # Test with very long prompt
        long_prompt = "def test(): " * 100
        result = agent.generate_code(long_prompt)
        assert isinstance(result, str)


if __name__ == "__main__":
    pytest.main([__file__, "-v", "-m", "smoke"])