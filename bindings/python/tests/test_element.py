from __future__ import annotations

import pytest

from quiver import Element


def test_element_set_integer() -> None:
    e = Element()
    try:
        e.set("value", 42)
    finally:
        e.destroy()


def test_element_set_float() -> None:
    e = Element()
    try:
        e.set("value", 3.14)
    finally:
        e.destroy()


def test_element_set_string() -> None:
    e = Element()
    try:
        e.set("label", "hello")
    finally:
        e.destroy()


def test_element_set_null() -> None:
    e = Element()
    try:
        e.set("optional", None)
    finally:
        e.destroy()


def test_element_set_bool_as_integer() -> None:
    e = Element()
    try:
        e.set("flag", True)
        e.set("other_flag", False)
    finally:
        e.destroy()


def test_element_fluent_chaining() -> None:
    e = Element().set("label", "x").set("value", 42).set("name", "test")
    try:
        assert isinstance(e, Element)
    finally:
        e.destroy()


def test_element_set_array_integer() -> None:
    e = Element()
    try:
        e.set("values", [1, 2, 3])
    finally:
        e.destroy()


def test_element_set_array_float() -> None:
    e = Element()
    try:
        e.set("values", [1.0, 2.0, 3.0])
    finally:
        e.destroy()


def test_element_set_array_string() -> None:
    e = Element()
    try:
        e.set("tags", ["a", "b", "c"])
    finally:
        e.destroy()


def test_element_set_empty_array() -> None:
    e = Element()
    try:
        e.set("values", [])
    finally:
        e.destroy()


def test_element_set_unsupported_type_raises() -> None:
    e = Element()
    try:
        with pytest.raises(TypeError, match="Unsupported type"):
            e.set("bad", {})
    finally:
        e.destroy()


def test_element_destroy_is_idempotent() -> None:
    e = Element()
    e.destroy()
    e.destroy()  # Should not raise


def test_element_repr() -> None:
    e = Element()
    try:
        e.set("label", "test")
        r = repr(e)
        assert isinstance(r, str)
        assert len(r) > 0
    finally:
        e.destroy()


def test_element_repr_destroyed() -> None:
    e = Element()
    e.destroy()
    assert repr(e) == "Element(destroyed)"


def test_element_clear() -> None:
    e = Element()
    try:
        e.set("label", "test").set("value", 42)
        e.clear()
        # After clear, can set new values
        e.set("label", "new_test").set("value", 99)
    finally:
        e.destroy()
