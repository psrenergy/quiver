"""Validate a wheel-installed quiverdb loads bundled native libraries."""

import sys


def validate() -> bool:
    ok = True

    # 1. Import quiverdb
    try:
        import quiverdb
    except ImportError as e:
        print(f"FAIL: import quiverdb failed: {e}")
        return False

    print("PASS: import quiverdb succeeded")

    # 2. Call version() first to trigger lazy library loading
    version = quiverdb.version()
    if version and isinstance(version, str):
        print(f"PASS: version() = '{version}'")
    else:
        print(f"FAIL: version() returned '{version}' (expected non-empty string)")
        ok = False

    # 3. Check _load_source is "bundled" (access via module attribute, not import,
    #    because the value is set during load_library() which runs lazily)
    import quiverdb._loader as loader

    load_source = loader._load_source
    libs_dir = loader._LIBS_DIR

    if load_source == "bundled":
        print(f"PASS: _load_source = '{load_source}'")
    else:
        print(f"FAIL: _load_source = '{load_source}' (expected 'bundled')")
        ok = False

    # 4. Diagnostics
    print()
    print("Diagnostics:")
    print(f"  _load_source: {load_source}")
    print(f"  version: {version}")
    print(f"  _libs dir: {libs_dir}")
    print(f"  _libs exists: {libs_dir.is_dir()}")

    print()
    if ok:
        print("VALIDATION PASSED - wheel-installed quiverdb uses bundled libraries")
    else:
        print("VALIDATION FAILED")

    return ok


if __name__ == "__main__":
    sys.exit(0 if validate() else 1)
