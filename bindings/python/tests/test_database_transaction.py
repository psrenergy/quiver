from __future__ import annotations

import pytest

from quiverdb import Database, Element


class TestExplicitTransaction:
    """Tests for explicit begin/commit/rollback transaction control."""

    def test_begin_and_commit_persists(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "Config"))
        collections_db.begin_transaction()
        collections_db.create_element(
            "Collection", Element().set("label", "Item 1").set("some_integer", 10),
        )
        collections_db.commit()

        labels = collections_db.read_scalar_strings("Collection", "label")
        assert len(labels) == 1
        assert labels[0] == "Item 1"

    def test_begin_and_rollback_discards(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "Config"))
        collections_db.begin_transaction()
        collections_db.create_element(
            "Collection", Element().set("label", "Item 1").set("some_integer", 10),
        )
        collections_db.rollback()

        ids = collections_db.read_element_ids("Collection")
        assert len(ids) == 0

    def test_in_transaction_false_by_default(self, collections_db: Database) -> None:
        assert collections_db.in_transaction() is False

    def test_in_transaction_true_after_begin(self, collections_db: Database) -> None:
        collections_db.begin_transaction()
        assert collections_db.in_transaction() is True
        collections_db.rollback()

    def test_in_transaction_false_after_commit(self, collections_db: Database) -> None:
        collections_db.begin_transaction()
        collections_db.commit()
        assert collections_db.in_transaction() is False


class TestTransactionContextManager:
    """Tests for the transaction() context manager."""

    def test_auto_commits_on_success(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "Config"))
        with collections_db.transaction() as db:
            db.create_element(
                "Collection", Element().set("label", "Item 1").set("some_integer", 42),
            )

        labels = collections_db.read_scalar_strings("Collection", "label")
        assert len(labels) == 1
        assert labels[0] == "Item 1"

    def test_auto_rollback_on_exception(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "Config"))
        try:
            with collections_db.transaction() as db:
                db.create_element(
                    "Collection", Element().set("label", "Item 1").set("some_integer", 10),
                )
                raise ValueError("intentional error")
        except ValueError:
            pass

        ids = collections_db.read_element_ids("Collection")
        assert len(ids) == 0

    def test_yields_same_db(self, collections_db: Database) -> None:
        with collections_db.transaction() as db:
            assert db is collections_db

    def test_re_raises_original_exception(self, collections_db: Database) -> None:
        with pytest.raises(ValueError, match="test error"):
            with collections_db.transaction() as db:
                raise ValueError("test error")

    def test_multiple_operations_in_transaction(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "Config"))
        with collections_db.transaction() as db:
            db.create_element(
                "Collection", Element().set("label", "Item 1").set("some_integer", 10),
            )
            db.update_scalar_integer("Collection", "some_integer", 1, 100)

        labels = collections_db.read_scalar_strings("Collection", "label")
        assert len(labels) == 1
        assert labels[0] == "Item 1"

        value = collections_db.read_scalar_integer_by_id("Collection", "some_integer", 1)
        assert value == 100
