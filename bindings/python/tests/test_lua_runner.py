"""Tests for LuaRunner."""
import pytest

from psr_database import Database, LuaRunner, LuaException


class TestLuaRunner:
    """Tests for LuaRunner Lua script execution."""

    def test_create_element_via_lua(self, basic_db: Database) -> None:
        """Create element using Lua script."""
        with LuaRunner(basic_db) as lua:
            lua.run('''
                db:create_element("Configuration", {
                    label = "lua_created",
                    integer_attribute = 42,
                    string_attribute = "from_lua"
                })
            ''')

        labels = basic_db.read_scalar_strings("Configuration", "label")
        assert "lua_created" in labels

        values = basic_db.read_scalar_integers("Configuration", "integer_attribute")
        assert 42 in values

    def test_read_values_via_lua(self, basic_db: Database) -> None:
        """Read values using Lua script."""
        basic_db.create_element("Configuration", {"label": "test1", "integer_attribute": 10})
        basic_db.create_element("Configuration", {"label": "test2", "integer_attribute": 20})

        with LuaRunner(basic_db) as lua:
            lua.run('''
                local values = db:read_scalar_integers("Configuration", "integer_attribute")
                -- Lua script can read and process values
                assert(#values == 2)
            ''')

    def test_lua_error_handling(self, basic_db: Database) -> None:
        """Lua errors are raised as LuaException."""
        with LuaRunner(basic_db) as lua:
            with pytest.raises(LuaException):
                lua.run('''
                    -- This will cause an error
                    error("intentional error")
                ''')

    def test_lua_syntax_error(self, basic_db: Database) -> None:
        """Lua syntax errors are raised as LuaException."""
        with LuaRunner(basic_db) as lua:
            with pytest.raises(LuaException):
                lua.run('''
                    this is not valid lua syntax
                ''')

    def test_multiple_script_runs(self, basic_db: Database) -> None:
        """Run multiple scripts with same LuaRunner."""
        with LuaRunner(basic_db) as lua:
            lua.run('''
                db:create_element("Configuration", { label = "first" })
            ''')
            lua.run('''
                db:create_element("Configuration", { label = "second" })
            ''')

        labels = basic_db.read_scalar_strings("Configuration", "label")
        assert "first" in labels
        assert "second" in labels

    def test_lua_runner_context_manager(self, basic_db: Database) -> None:
        """Test LuaRunner context manager."""
        with LuaRunner(basic_db) as lua:
            lua.run('db:create_element("Configuration", { label = "test" })')
        # LuaRunner should be disposed after exiting context


class TestLuaRunnerVectorsAndSets:
    """Tests for LuaRunner with vectors and sets."""

    def test_create_with_vector(self, collections_db: Database) -> None:
        """Create element with vector via Lua."""
        with LuaRunner(collections_db) as lua:
            lua.run('''
                db:create_element("Configuration", { label = "test_config" })
                db:create_element("Collection", {
                    label = "lua_vector",
                    value_int = {1, 2, 3, 4, 5}
                })
            ''')

        vectors = collections_db.read_vector_integers("Collection", "value_int")
        assert [1, 2, 3, 4, 5] in vectors

    def test_create_with_set(self, collections_db: Database) -> None:
        """Create element with set via Lua."""
        with LuaRunner(collections_db) as lua:
            lua.run('''
                db:create_element("Configuration", { label = "test_config" })
                db:create_element("Collection", {
                    label = "lua_tags",
                    tag = {"tag_a", "tag_b"}
                })
            ''')

        sets = collections_db.read_set_strings("Collection", "tag")
        assert any(set(s) == {"tag_a", "tag_b"} for s in sets)

    def test_read_vectors_via_lua(self, collections_db: Database) -> None:
        """Read vector values via Lua."""
        collections_db.create_element("Configuration", {"label": "test_config"})
        collections_db.create_element("Collection", {"label": "test", "value_int": [10, 20, 30]})

        with LuaRunner(collections_db) as lua:
            lua.run('''
                local vectors = db:read_vector_integers("Collection", "value_int")
                assert(#vectors == 1)
                assert(#vectors[1] == 3)
            ''')
