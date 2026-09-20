from __future__ import annotations

from dataclasses import dataclass, field
from enum import IntEnum


class DataType(IntEnum):
    """Data type constants matching C API quiver_data_type_t."""

    INTEGER = 0
    FLOAT = 1
    STRING = 2
    DATE_TIME = 3
    NULL = 4


class LogLevel(IntEnum):
    """Console log level constants matching C API quiver_log_level_t."""

    DEBUG = 0
    INFO = 1
    WARN = 2
    ERROR = 3
    OFF = 4


@dataclass(frozen=True)
class CSVOptions:
    """Options for CSV import and export operations."""

    date_time_format: str = ""
    enum_labels: dict[str, dict[str, dict[str, int]]] = field(default_factory=dict)


@dataclass(frozen=True)
class ScalarMetadata:
    """Metadata for a scalar attribute in a collection."""

    name: str
    data_type: DataType
    not_null: bool
    primary_key: bool
    default_value: str | None
    is_foreign_key: bool
    references_collection: str | None
    references_column: str | None


@dataclass(frozen=True)
class GroupMetadata:
    """Metadata for a vector, set, or time series group in a collection."""

    group_name: str
    dimension_column: str  # empty string for vector/set groups
    value_columns: list[ScalarMetadata]


@dataclass(frozen=True)
class UiMetadata:
    """PSR `<db_dir>/ui/` sidecar record for one scalar attribute (Phase 3, META-01/META-02).

    Every string field is a plain `str`, never `str | None` -- D-13 makes empty string the
    spelling of absence on every FFI layer. `configured` is the discriminator between "the
    sidecar declares this" and "nothing declared"; `label` is never back-filled from an id, so a
    declared-but-blank label (`configured is True`, `label == ""`) is distinct from an
    unconfigured attribute (`configured is False`, every string field empty).
    """

    configured: bool
    label: str
    tooltip: str
    unit: str
    format: str
    icon: str
    hidden: bool
    vocabulary: str
    display_order: int


@dataclass(frozen=True)
class UiEnumEntry:
    """One entry in an enum vocabulary (Phase 3, META-05). Returned in declaration order by
    Database.get_ui_vocabulary -- never a null cell, since D-11 crosses the vocabulary as two
    parallel arrays rather than a struct with optional fields."""

    code: int
    label: str
