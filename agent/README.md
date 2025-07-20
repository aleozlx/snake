# Interactive Code Generation Agents

Clean, testable agents using dependency injection pattern.

## Quick Start

```bash
# Run tests
pytest tests/                       # All 25 tests
pytest tests/ -m smoke              # Core tests only

# Use agents
python run_agent.py --agent-type a1                             # Basic batch generation (VLLM default)
python run_agent.py --agent-type a2                             # Interactive agent (natural language + VLLM default)
python run_agent.py --agent-type a2 --code-completion           # Code completion mode
python run_agent.py --agent-type a2 --backend mock              # Development/testing with mock backend
```

## Architecture

```
Agent Classes (a1, a2, a3)
    ↓ depends on
Backend Interface (Abstract)
    ↓ implemented by
Concrete Backends (VLLM, Mock, File)
```

## Key Files

- `backend.py` - Backend abstractions and implementations
- `factory.py` - Easy agent creation with different backends
- `a1.py` - Basic batch code generation agent
- `a2.py` - Interactive agent with natural language support (use --code-completion for basic mode)
- `run_agent.py` - Universal runner for all agent types
- `tests/test_agents.py` - Agent tests (18 tests)
- `tests/test_utilities.py` - Utility tests (7 tests)

## Usage

```python
# Create agents
from factory import create_mock_agent, create_agent

agent = create_agent("a2")                              # VLLM backend + natural language (defaults)
agent = create_agent("a2", natural_language=False)        # VLLM backend + code completion mode  
agent = create_mock_agent("a2")                           # Mock backend for testing

# Generate code
result = agent.generate_code("def fibonacci")
```

## Testing

Simple mock backend approach - no complex third-party mocking:

```python
backend = MockBackend({"def test": "def test():\n    pass"})
agent = InteractiveCodeGenerator(backend)
result = agent.generate_code("def test")  # Predictable!
```

Status: ✅ 25/25 tests passing