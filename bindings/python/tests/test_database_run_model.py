from __future__ import annotations

from pathlib import Path

import pytest

from quiverdb import Database, QuiverError


class TestRunModel:
    """Tests for run_model().

    The happy path (a real exit code from CarbSteeler) is not testable hermetically because
    the model directory is a hardcoded constant in the native layer; it is verified manually.
    The in-memory precondition fires before the model directory is consulted, so it is
    deterministic on every machine/CI runner.
    """

    def test_run_model_raises_on_memory_database(self, collections_schema_path: Path) -> None:
        db = Database.from_schema(":memory:", str(collections_schema_path))
        try:
            with pytest.raises(QuiverError, match="in-memory database"):
                db.run_model()
        finally:
            db.close()
