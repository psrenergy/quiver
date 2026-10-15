from __future__ import annotations

import importlib.util
import os
import sys
from pathlib import Path
from unittest.mock import Mock, call

import pytest

import quiverdb._loader as loader


@pytest.mark.parametrize(
    ("platform", "core", "c_api"),
    [
        ("win32", "libquiver.dll", "libquiver_c.dll"),
        ("linux", "libquiver.so", "libquiver_c.so"),
        ("darwin", "libquiver.0.dylib", "libquiver_c.dylib"),
    ],
)
@pytest.mark.parametrize("bundled", [True, False])
def test_library_paths(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path, platform: str, core: str, c_api: str, bundled: bool
) -> None:
    spec = importlib.util.spec_from_file_location("test_loader", loader.__file__)
    module = importlib.util.module_from_spec(spec)
    ffi = Mock()

    with monkeypatch.context() as patch:
        patch.setattr(os, "add_dll_directory", Mock(), raising=False)
        patch.setattr(sys, "platform", platform)
        spec.loader.exec_module(module)
        patch.setattr(module, "_LIBS_DIR", tmp_path)
        if bundled:
            (tmp_path / core).touch()
            (tmp_path / c_api).touch()
            paths = [str(tmp_path / core), str(tmp_path / c_api)]
        else:
            paths = ["libquiver", "libquiver_c"] if platform == "win32" else [core, c_api]

        assert module.load_library(ffi) is ffi.dlopen.return_value

    assert ffi.dlopen.call_args_list == [call(path) for path in paths]
    assert module._load_source == ("bundled" if bundled else "development")
