from __future__ import annotations

import pytest

import quiverdb
from quiverdb._c_api import ffi, get_lib
from quiverdb._loader import _check_struct_size

# SAFE-02/SAFE-03 (Phase 2, plan 02-03/02-08; Phase 3, plan 03-01 D-40): the load-time struct-size
# gate must ACTIVELY CALL the five native *_sizeof() accessors (CFFI ABI mode resolves lib.<name>
# via dlsym-on-demand, so a missing symbol only raises at the call, never at ffi.dlopen --
# RESEARCH Pitfall 4) and the failure path -- a wrong layout actually raising -- must be proven by
# a test, not just the happy path asserted against itself.
#
# 02-08: none of the tests below may call _assert_struct_sizes directly. Doing so would populate
# _CHECKED_STRUCTS itself and re-create the exact defect this file shipped with in HEAD -- both
# call sites in _loader.py were once found silently replaced with a no-op statement, and every
# test here still passed because they drove _check_struct_size / the accessors directly, never the
# wired gate. test_gate_ran_and_checked_all_five_structs_in_order below is the one that would
# have caught it: it observes the evidence the *import* left behind.


def test_native_accessors_match_cdef_sizes() -> None:
    """The happy path: importing/loading the package does not raise, and all five sizes agree."""
    lib = get_lib()
    assert lib.quiver_database_options_sizeof() == ffi.sizeof("quiver_database_options_t") == 24
    assert lib.quiver_scalar_metadata_sizeof() == ffi.sizeof("quiver_scalar_metadata_t") == 56
    assert lib.quiver_group_metadata_sizeof() == ffi.sizeof("quiver_group_metadata_t") == 32
    assert lib.quiver_csv_options_sizeof() == ffi.sizeof("quiver_csv_options_t") == 56
    assert lib.quiver_ui_metadata_sizeof() == ffi.sizeof("quiver_ui_metadata_t") == 64


def test_gate_ran_and_checked_all_five_structs_in_order() -> None:
    """Observes the wiring evidence _assert_struct_sizes leaves behind on library load -- does
    NOT call _assert_struct_sizes itself. get_lib() forces the load (idempotent if already
    loaded by an earlier test); the gate runs as a side effect of load_library, exactly as it
    would for any real caller. An empty or partial list here means the gate is unwired or
    short-circuited, which is exactly the defect that shipped in this file in HEAD."""
    get_lib()
    assert quiverdb._loader._CHECKED_STRUCTS == [
        "quiver_database_options_t",
        "quiver_scalar_metadata_t",
        "quiver_group_metadata_t",
        "quiver_csv_options_t",
        "quiver_ui_metadata_t",
    ]


def test_check_struct_size_passes_on_equal_sizes() -> None:
    assert _check_struct_size("quiver_database_options_t", 24, 24) is None


@pytest.mark.parametrize(
    ("struct_name", "expected", "native"),
    [
        ("quiver_database_options_t", 24, 8),
        ("quiver_scalar_metadata_t", 56, 40),
        ("quiver_group_metadata_t", 32, 16),
        ("quiver_csv_options_t", 56, 55),
        ("quiver_csv_options_t", 56, 57),
    ],
)
def test_check_struct_size_raises_on_mismatch(struct_name: str, expected: int, native: int) -> None:
    """Proves the failure path actually fires: a deliberately wrong expected value raises,
    naming the struct and BOTH numbers in the message -- criterion 5's whole point."""
    with pytest.raises(RuntimeError, match=struct_name) as exc_info:
        _check_struct_size(struct_name, expected, native)
    message = str(exc_info.value)
    assert str(expected) in message
    assert str(native) in message
