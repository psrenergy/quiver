import 'dart:ffi';

import 'package:ffi/ffi.dart';

import 'ffi/bindings.dart';

/// Metadata for a scalar attribute in a collection.
class ScalarMetadata {
  final String name;
  final int dataType;
  final bool notNull;
  final bool primaryKey;
  final String? defaultValue;
  final bool isForeignKey;
  final String? referencesCollection;
  final String? referencesColumn;

  const ScalarMetadata({
    required this.name,
    required this.dataType,
    required this.notNull,
    required this.primaryKey,
    required this.defaultValue,
    required this.isForeignKey,
    required this.referencesCollection,
    required this.referencesColumn,
  });

  /// Creates a ScalarMetadata from a native quiver_scalar_metadata_t struct.
  ScalarMetadata.fromNative(quiver_scalar_metadata_t native)
      : name = native.name.cast<Utf8>().toDartString(),
        dataType = native.data_type,
        notNull = native.not_null != 0,
        primaryKey = native.primary_key != 0,
        defaultValue = native.default_value == nullptr
            ? null
            : native.default_value.cast<Utf8>().toDartString(),
        isForeignKey = native.is_foreign_key != 0,
        referencesCollection = native.references_collection == nullptr
            ? null
            : native.references_collection.cast<Utf8>().toDartString(),
        referencesColumn = native.references_column == nullptr
            ? null
            : native.references_column.cast<Utf8>().toDartString();
}

/// Metadata for a vector, set, or time series group in a collection.
class GroupMetadata {
  final String groupName;
  final String dimensionColumn;
  final List<ScalarMetadata> valueColumns;

  const GroupMetadata({
    required this.groupName,
    required this.dimensionColumn,
    required this.valueColumns,
  });

  /// Creates a GroupMetadata from a native quiver_group_metadata_t struct.
  GroupMetadata.fromNative(quiver_group_metadata_t native)
      : groupName = native.group_name.cast<Utf8>().toDartString(),
        dimensionColumn = native.dimension_column == nullptr
            ? ''
            : native.dimension_column.cast<Utf8>().toDartString(),
        valueColumns = List.generate(
          native.value_column_count,
          (i) => ScalarMetadata.fromNative(native.value_columns[i]),
        );
}
