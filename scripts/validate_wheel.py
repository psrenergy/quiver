"""Validate a quiverdb wheel contains the expected native libraries."""

import sys
import os
import zipfile


def validate(whl_path: str) -> bool:
    print(f"Inspecting: {os.path.basename(whl_path)}")
    print()

    with zipfile.ZipFile(whl_path, "r") as z:
        names = z.namelist()

        # Determine platform extensions
        if sys.platform == "win32":
            libs = [
                "quiverdb/_libs/libquiver.dll",
                "quiverdb/_libs/libquiver_c.dll",
            ]
        else:
            libs = [
                "quiverdb/_libs/libquiver.so",
                "quiverdb/_libs/libquiver_c.so",
            ]

        # Check required libraries
        ok = True
        for lib in libs:
            if lib in names:
                info = z.getinfo(lib)
                size_kb = info.file_size / 1024
                print(f"  PASS: {lib} ({size_kb:.1f} KB)")
            else:
                print(f"  FAIL: {lib} NOT FOUND")
                ok = False

        print()

        # Check for stray directories
        stray_lib = [n for n in names if n.startswith("lib/")]
        stray_bin = [n for n in names if n.startswith("bin/")]

        if stray_lib:
            print(f"  FAIL: stray lib/ directory ({len(stray_lib)} entries)")
            ok = False
        else:
            print("  PASS: no stray lib/ directory")

        if stray_bin:
            print(f"  FAIL: stray bin/ directory ({len(stray_bin)} entries)")
            ok = False
        else:
            print("  PASS: no stray bin/ directory")

        print()

        # List _libs/ contents for reference
        libs_entries = sorted([n for n in names if "_libs/" in n])
        if libs_entries:
            print("Wheel _libs/ contents:")
            for entry in libs_entries:
                info = z.getinfo(entry)
                print(f"  {entry} ({info.file_size / 1024:.1f} KB)")
        print()

        if not ok:
            print("VALIDATION FAILED")
        else:
            print("VALIDATION PASSED - wheel contents are correct")

    return ok


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <wheel-file>")
        sys.exit(2)
    sys.exit(0 if validate(sys.argv[1]) else 1)
