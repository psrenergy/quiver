from __future__ import annotations

import json

import pytest

from quiverdb import Database, LuaRunner, QuiverError


class TestLuaRunnerCreateRead:
    """Tests for Lua scripts that create and read elements."""

    def test_create_element_from_lua(self, collections_db: Database) -> None:
        lua = LuaRunner(collections_db)
        lua.run("""
            db:create_element("Configuration", { label = "default" })
            db:create_element("Collection", { label = "Item1", some_integer = 42, some_float = 3.14 })
        """)
        labels = collections_db.read_scalar_strings("Collection", "label")
        assert labels == ["Item1"]
        values = collections_db.read_scalar_integers("Collection", "some_integer")
        assert values == [42]
        lua.close()

    def test_read_scalars_from_lua(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="default")
        collections_db.create_element("Collection", label="Seeded", some_integer=99)
        lua = LuaRunner(collections_db)
        lua.run("""
            local labels = db:read_scalar_strings("Collection", "label")
            assert(#labels == 1, "Expected 1 label, got " .. #labels)
            assert(labels[1] == "Seeded", "Expected 'Seeded', got " .. labels[1])
        """)
        lua.close()


class TestLuaRunnerErrors:
    """Tests for Lua script error handling."""

    def test_syntax_error_raises_quiver_error(self, collections_db: Database) -> None:
        lua = LuaRunner(collections_db)
        with pytest.raises(QuiverError) as exc_info:
            lua.run("invalid syntax !!!")
        assert str(exc_info.value) != ""
        lua.close()

    def test_runtime_error_raises_quiver_error(self, collections_db: Database) -> None:
        lua = LuaRunner(collections_db)
        with pytest.raises(QuiverError):
            lua.run("print(undefined_variable.field)")
        lua.close()

    def test_invalid_collection_raises_quiver_error(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="default")
        lua = LuaRunner(collections_db)
        with pytest.raises(QuiverError):
            lua.run("""
                db:create_element("NonexistentCollection", { label = "Bad" })
            """)
        lua.close()


class TestLuaRunnerLifecycle:
    """Tests for LuaRunner lifecycle management."""

    def test_multiple_run_calls(self, collections_db: Database) -> None:
        lua = LuaRunner(collections_db)
        lua.run('db:create_element("Configuration", { label = "default" })')
        lua.run('db:create_element("Collection", { label = "Item1", some_integer = 1 })')
        lua.run('db:create_element("Collection", { label = "Item2", some_integer = 2 })')
        labels = collections_db.read_scalar_strings("Collection", "label")
        assert sorted(labels) == ["Item1", "Item2"]
        lua.close()

    def test_empty_script(self, collections_db: Database) -> None:
        lua = LuaRunner(collections_db)
        lua.run("")
        lua.close()

    def test_comment_only_script(self, collections_db: Database) -> None:
        lua = LuaRunner(collections_db)
        lua.run("-- just a comment")
        lua.close()

    def test_context_manager(self, collections_db: Database) -> None:
        with LuaRunner(collections_db) as lua:
            lua.run('db:create_element("Configuration", { label = "default" })')
            labels = collections_db.read_scalar_strings("Configuration", "label")
            assert labels == ["default"]
        with pytest.raises(QuiverError, match="LuaRunner is closed"):
            lua.run("-- should fail")

    def test_run_after_close_raises(self, collections_db: Database) -> None:
        lua = LuaRunner(collections_db)
        lua.close()
        with pytest.raises(QuiverError, match="LuaRunner is closed"):
            lua.run("-- should fail")

    def test_close_idempotent(self, collections_db: Database) -> None:
        lua = LuaRunner(collections_db)
        lua.close()
        lua.close()  # Should not raise

    def test_database_reference_kept(self, collections_db: Database) -> None:
        lua = LuaRunner(collections_db)
        assert lua._db is collections_db
        lua.close()


class TestLuaRunnerReturnValues:
    """A script hands one value back to the caller as JSON."""

    def test_returns_json(self, collections_db: Database) -> None:
        lua = LuaRunner(collections_db)
        assert lua.run("return { a = 1, b = { 2, 3 } }") == '{"a":1,"b":[2,3]}'
        assert json.loads(lua.run('return db:read_element_ids("Collection")')) == []

    def test_returns_empty_string_when_script_returns_nothing(self, collections_db: Database) -> None:
        lua = LuaRunner(collections_db)
        assert lua.run("local x = 1") == ""


class TestDatabaseDryRun:
    """A dry run executes writes and throws them away."""

    def test_rolls_back_a_script(self, collections_db: Database) -> None:
        lua = LuaRunner(collections_db)
        lua.run('db:create_element("Configuration", { label = "default" })')

        assert collections_db.in_dry_run() is False
        with collections_db.dry_run():
            assert collections_db.in_dry_run() is True
            # db:transaction composes: the dry run absorbs the nested BEGIN/COMMIT.
            result = lua.run("""
                db:transaction(function(db)
                    db:create_element("Collection", { label = "Preview" })
                end)
                return db:read_scalar_strings("Collection", "label")
            """)
            assert json.loads(result) == ["Preview"]

        assert collections_db.in_dry_run() is False
        assert collections_db.read_scalar_strings("Collection", "label") == []

    def test_rolls_back_on_exception(self, collections_db: Database) -> None:
        lua = LuaRunner(collections_db)
        lua.run('db:create_element("Configuration", { label = "default" })')

        with pytest.raises(ValueError):
            with collections_db.dry_run():
                lua.run('db:create_element("Collection", { label = "Preview" })')
                raise ValueError("boom")

        assert collections_db.read_scalar_strings("Collection", "label") == []
        assert collections_db.in_dry_run() is False

    def test_end_without_start_raises(self, collections_db: Database) -> None:
        with pytest.raises(QuiverError, match="no active dry run"):
            collections_db.end_dry_run()
