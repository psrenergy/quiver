from __future__ import annotations

import pytest

from quiverdb import Database


def test_boolean_convenience_methods(all_types_db: Database) -> None:
    assert all_types_db.read_scalar_booleans("AllTypes", "some_integer") == []
    assert all_types_db.read_vector_booleans("AllTypes", "count_value") == []
    assert all_types_db.read_set_booleans("AllTypes", "code") == []

    id_false = all_types_db.create_element(
        "AllTypes",
        label="False",
        some_integer=0,
        count_value=[0, 1],
        code=[0, 1],
    )
    id_true = all_types_db.create_element(
        "AllTypes",
        label="True",
        some_integer=1,
        count_value=[1, 0],
        code=[1],
    )
    id_null = all_types_db.create_element("AllTypes", label="Null")

    assert all_types_db.read_scalar_booleans("AllTypes", "some_integer") == [False, True, None]
    assert all_types_db.read_scalar_boolean_by_id("AllTypes", "some_integer", id_false) is False
    assert all_types_db.read_scalar_boolean_by_id("AllTypes", "some_integer", id_true) is True
    assert all_types_db.read_scalar_boolean_by_id("AllTypes", "some_integer", id_null) is None

    assert all_types_db.read_vector_booleans("AllTypes", "count_value") == [[False, True], [True, False]]
    assert all_types_db.read_vector_booleans_by_id("AllTypes", "count_value", id_false) == [False, True]
    assert all_types_db.read_vector_booleans_by_id("AllTypes", "count_value", id_null) == []

    assert all_types_db.read_set_booleans("AllTypes", "code") == [[False, True], [True]]
    assert all_types_db.read_set_booleans_by_id("AllTypes", "code", id_false) == [False, True]
    assert all_types_db.read_set_booleans_by_id("AllTypes", "code", id_null) == []

    assert all_types_db.query_boolean("SELECT 0") is False
    assert (
        all_types_db.query_boolean(
            "SELECT some_integer FROM AllTypes WHERE id = ?",
            parameters=[id_true],
        )
        is True
    )
    assert all_types_db.query_boolean("SELECT some_integer FROM AllTypes WHERE id = -1") is None


def test_boolean_conversion_rejects_non_binary_integer(all_types_db: Database) -> None:
    id_invalid = all_types_db.create_element(
        "AllTypes",
        label="Invalid",
        some_integer=2,
        count_value=[0, 2],
        code=[2],
    )

    with pytest.raises(ValueError, match="AllTypes.some_integer.*expected 0 or 1"):
        all_types_db.read_scalar_booleans("AllTypes", "some_integer")

    with pytest.raises(ValueError, match="AllTypes.some_integer.*expected 0 or 1"):
        all_types_db.read_scalar_boolean_by_id("AllTypes", "some_integer", id_invalid)

    with pytest.raises(ValueError, match="AllTypes.count_value.*expected 0 or 1"):
        all_types_db.read_vector_booleans("AllTypes", "count_value")

    with pytest.raises(ValueError, match="AllTypes.count_value.*expected 0 or 1"):
        all_types_db.read_vector_booleans_by_id("AllTypes", "count_value", id_invalid)

    with pytest.raises(ValueError, match="AllTypes.code.*expected 0 or 1"):
        all_types_db.read_set_booleans("AllTypes", "code")

    with pytest.raises(ValueError, match="AllTypes.code.*expected 0 or 1"):
        all_types_db.read_set_booleans_by_id("AllTypes", "code", id_invalid)

    with pytest.raises(ValueError, match="expected 0 or 1"):
        all_types_db.query_boolean("SELECT 2")


def test_boolean_input(all_types_db: Database) -> None:
    """A native bool on the write side.

    Python needs no special handling in most places (`bool` is an `int` subclass), but
    `Element.set` and the group/row marshallers each test `bool` explicitly and before `int`,
    so a stray reordering would send a bool down the float or the unsupported-type path.
    """
    element_id = all_types_db.create_element(
        "AllTypes",
        label="Written",
        some_integer=True,
        count_value=[True, False],
        code=[True],
    )

    assert all_types_db.read_scalar_boolean_by_id("AllTypes", "some_integer", element_id) is True
    assert all_types_db.read_vector_booleans_by_id("AllTypes", "count_value", element_id) == [True, False]
    assert all_types_db.read_set_booleans_by_id("AllTypes", "code", element_id) == [True]

    all_types_db.update_element("AllTypes", element_id, some_integer=False)
    assert all_types_db.read_scalar_boolean_by_id("AllTypes", "some_integer", element_id) is False

    all_types_db.update_element_by_label("AllTypes", "Written", some_integer=True)
    assert all_types_db.read_scalar_boolean_by_id("AllTypes", "some_integer", element_id) is True

    assert (
        all_types_db.query_boolean("SELECT some_integer FROM AllTypes WHERE some_integer = ?", parameters=[True])
        is True
    )

    all_types_db.update_vector_group("AllTypes", "counts", element_id, {"count_value": [True, False, True]})
    assert all_types_db.read_vector_booleans_by_id("AllTypes", "count_value", element_id) == [
        True,
        False,
        True,
    ]

    all_types_db.update_set_group("AllTypes", "codes", element_id, {"code": [True, False]})
    # A set has no insertion order.
    assert sorted(all_types_db.read_set_booleans_by_id("AllTypes", "code", element_id)) == [False, True]
