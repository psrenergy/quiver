from __future__ import annotations

import gc
import json
import sys
import warnings

import pytest

from quiverdb import Database, QuiverError, Sandbox


class TestSandboxCreateRead:
    """Tests for scripts that create and read elements."""

    def test_create_element_from_sandbox(self, collections_db: Database) -> None:
        sandbox = Sandbox(collections_db)
        sandbox.run("""
            db:create_element("Configuration", { label = "default" })
            db:create_element("Collection", { label = "Item1", some_integer = 42, some_float = 3.14 })
        """)
        labels = collections_db.read_scalar_strings("Collection", "label")
        assert labels == ["Item1"]
        values = collections_db.read_scalar_integers("Collection", "some_integer")
        assert values == [42]
        sandbox.close()

    def test_read_scalars_from_sandbox(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="default")
        collections_db.create_element("Collection", label="Seeded", some_integer=99)
        sandbox = Sandbox(collections_db)
        sandbox.run("""
            local labels = db:read_scalar_strings("Collection", "label")
            assert(#labels == 1, "Expected 1 label, got " .. #labels)
            assert(labels[1] == "Seeded", "Expected 'Seeded', got " .. labels[1])
        """)
        sandbox.close()


class TestSandboxErrors:
    """Tests for script error handling."""

    def test_syntax_error_raises_quiver_error(self, collections_db: Database) -> None:
        sandbox = Sandbox(collections_db)
        with pytest.raises(QuiverError) as exc_info:
            sandbox.run("invalid syntax !!!")
        assert str(exc_info.value) != ""
        sandbox.close()

    def test_runtime_error_raises_quiver_error(self, collections_db: Database) -> None:
        sandbox = Sandbox(collections_db)
        with pytest.raises(QuiverError):
            sandbox.run("print(undefined_variable.field)")
        sandbox.close()

    def test_invalid_collection_raises_quiver_error(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="default")
        sandbox = Sandbox(collections_db)
        with pytest.raises(QuiverError):
            sandbox.run("""
                db:create_element("NonexistentCollection", { label = "Bad" })
            """)
        sandbox.close()


class TestSandboxLifecycle:
    """Tests for Sandbox lifecycle management."""

    def test_multiple_run_calls(self, collections_db: Database) -> None:
        sandbox = Sandbox(collections_db)
        sandbox.run('db:create_element("Configuration", { label = "default" })')
        sandbox.run('db:create_element("Collection", { label = "Item1", some_integer = 1 })')
        sandbox.run('db:create_element("Collection", { label = "Item2", some_integer = 2 })')
        labels = collections_db.read_scalar_strings("Collection", "label")
        assert sorted(labels) == ["Item1", "Item2"]
        sandbox.close()

    def test_empty_script(self, collections_db: Database) -> None:
        sandbox = Sandbox(collections_db)
        sandbox.run("")
        sandbox.close()

    def test_comment_only_script(self, collections_db: Database) -> None:
        sandbox = Sandbox(collections_db)
        sandbox.run("-- just a comment")
        sandbox.close()

    def test_context_manager(self, collections_db: Database) -> None:
        with Sandbox(collections_db) as sandbox:
            sandbox.run('db:create_element("Configuration", { label = "default" })')
            labels = collections_db.read_scalar_strings("Configuration", "label")
            assert labels == ["default"]
        with pytest.raises(QuiverError, match="Sandbox is closed"):
            sandbox.run("-- should fail")

    def test_run_after_close_raises(self, collections_db: Database) -> None:
        sandbox = Sandbox(collections_db)
        sandbox.close()
        with pytest.raises(QuiverError, match="Sandbox is closed"):
            sandbox.run("-- should fail")

    def test_close_idempotent(self, collections_db: Database) -> None:
        sandbox = Sandbox(collections_db)
        sandbox.close()
        sandbox.close()  # Should not raise

    def test_failed_construction_is_silent_when_collected(
        self, collections_db: Database, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        collections_db.close()
        gc.collect()  # flush earlier tests' garbage so only this runner's __del__ is observed
        unraisable: list[sys.UnraisableHookArgs] = []
        monkeypatch.setattr(sys, "unraisablehook", unraisable.append)
        with warnings.catch_warnings(record=True) as caught:
            warnings.simplefilter("always")  # ResourceWarning is ignored by the default filters
            with pytest.raises(QuiverError, match="Null argument: db"):
                Sandbox(collections_db)
            gc.collect()  # runs the half-built runner's __del__ even if it sits in a cycle
        assert [str(w.message) for w in caught] == []
        assert [repr(u.exc_value) for u in unraisable] == []

    def test_database_reference_kept(self, collections_db: Database) -> None:
        sandbox = Sandbox(collections_db)
        assert sandbox._db is collections_db
        sandbox.close()


class TestSandboxReturnValues:
    """A script hands one value back to the caller as JSON."""

    def test_returns_json(self, collections_db: Database) -> None:
        sandbox = Sandbox(collections_db)
        assert sandbox.run("return { a = 1, b = { 2, 3 } }") == '{"a":1,"b":[2,3]}'
        assert json.loads(sandbox.run('return db:read_element_ids("Collection")')) == []

    def test_returns_empty_string_when_script_returns_nothing(self, collections_db: Database) -> None:
        sandbox = Sandbox(collections_db)
        assert sandbox.run("local x = 1") == ""


class TestDatabaseDryRun:
    """A dry run executes writes and throws them away."""

    def test_rolls_back_a_script(self, collections_db: Database) -> None:
        sandbox = Sandbox(collections_db)
        sandbox.run('db:create_element("Configuration", { label = "default" })')

        assert collections_db.in_dry_run() is False
        with collections_db.dry_run():
            assert collections_db.in_dry_run() is True
            # db:transaction composes: the dry run absorbs the nested BEGIN/COMMIT.
            result = sandbox.run("""
                db:transaction(function(db)
                    db:create_element("Collection", { label = "Preview" })
                end)
                return db:read_scalar_strings("Collection", "label")
            """)
            assert json.loads(result) == ["Preview"]

        assert collections_db.in_dry_run() is False
        assert collections_db.read_scalar_strings("Collection", "label") == []

    def test_rolls_back_on_exception(self, collections_db: Database) -> None:
        sandbox = Sandbox(collections_db)
        sandbox.run('db:create_element("Configuration", { label = "default" })')

        with pytest.raises(ValueError):
            with collections_db.dry_run():
                sandbox.run('db:create_element("Collection", { label = "Preview" })')
                raise ValueError("boom")

        assert collections_db.read_scalar_strings("Collection", "label") == []
        assert collections_db.in_dry_run() is False

    def test_end_without_start_raises(self, collections_db: Database) -> None:
        with pytest.raises(QuiverError, match="no active dry run"):
            collections_db.end_dry_run()
