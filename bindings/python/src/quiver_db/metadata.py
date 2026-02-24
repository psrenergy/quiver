from __future__ import annotations

from dataclasses import dataclass, field


@dataclass(frozen=True)
class CSVExportOptions:
    """Options for CSV export operations."""

    date_time_format: str = ""
    enum_labels: dict[str, dict[int, str]] = field(default_factory=dict)


@dataclass(frozen=True)
class ScalarMetadata:
    """Metadata for a scalar attribute in a collection."""

    name: str
    data_type: int
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
