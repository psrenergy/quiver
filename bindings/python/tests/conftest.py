"""Pytest fixtures for PSR Database tests."""

from pathlib import Path

import pytest

SCHEMAS_PATH = Path(__file__).parent.parent.parent.parent / "tests" / "schemas"


@pytest.fixture
def schemas_path() -> Path:
    return SCHEMAS_PATH
