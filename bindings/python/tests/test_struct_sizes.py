from __future__ import annotations

import pytest

from quiverdb._c_api import ffi, get_lib
from quiverdb._loader import _check_struct_size

# SAFE-02/SAFE-03 (Phase 2, plan 02-03): the load-time struct-size gate must ACTIVELY CALL the
# three native *_sizeof() accessors (CFFI ABI mode resolves lib.<name> via dlsym-on-demand, so a
# missing symbol only raises at the call, never at ffi.dlopen -- RESEARCH Pitfall 4) and the
# failure path -- a wrong layout actually raising -- must be proven by a test, not just the happy
# path asserted against itself.


def test_native_accessors_match_cdef_sizes() -> None:
    """The happy path: importing/loading the package does not raise, and all three sizes agree."""
    lib = get_lib()
    assert lib.quiver_database_options_sizeof() == ffi.sizeof("quiver_database_options_t") == 24
    assert lib.quiver_scalar_metadata_sizeof() == ffi.sizeof("quiver_scalar_metadata_t") == 56
    assert lib.quiver_group_metadata_sizeof() == ffi.sizeof("quiver_group_metadata_t") == 32


def test_check_struct_size_passes_on_equal_sizes() -> None:
    assert _check_struct_size("quiver_database_options_t", 24, 24) is None


@pytest.mark.parametrize(
    ("struct_name", "expected", "native"),
    [
        ("quiver_database_options_t", 24, 8),
        ("quiver_scalar_metadata_t", 56, 40),
        ("quiver_group_metadata_t", 32, 16),
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
