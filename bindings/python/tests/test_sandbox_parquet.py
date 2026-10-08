from pathlib import Path

from quiverdb import Database, Sandbox


def test_parquet_snapshot(collections_db: Database, tmp_path: Path) -> None:
    with Sandbox(collections_db) as sandbox:
        sandbox.run("""
            local md = quiver.metadata{initial_datetime='2024-01-01T00:00:00', unit='MW',
                dimensions={'row'}, dimension_sizes={3}, labels={'value'}}
            local f = db:open_file('snapshot', 'w', md)
            f:write({1.5}, {row=2})
            f:close()
            db:bin_to_parquet('snapshot')
        """)
    with (tmp_path / "snapshot.parquet").open("rb") as snapshot:
        assert snapshot.read(4) == b"PAR1"
        snapshot.seek(-4, 2)
        assert snapshot.read(4) == b"PAR1"
    assert (tmp_path / "snapshot.qvr").is_file()
    assert (tmp_path / "snapshot.toml").is_file()
