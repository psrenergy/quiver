from __future__ import annotations

from quiverdb._c_api import get_lib
from quiverdb._helpers import check
from quiverdb.database_options import _marshal_csv_options
from quiverdb.metadata import CSVOptions


class DatabaseCSVExport:
    def export_csv(
        self,
        collection: str,
        group: str,
        path: str,
        *,
        options: CSVOptions | None = None,
    ) -> None:
        """Export a collection or group to CSV file."""
        self._ensure_open()
        lib = get_lib()
        if options is None:
            options = CSVOptions()
        keepalive, c_opts = _marshal_csv_options(options)
        check(
            lib.quiver_database_export_csv(
                self._ptr, collection.encode("utf-8"), group.encode("utf-8"), path.encode("utf-8"), c_opts
            )
        )
