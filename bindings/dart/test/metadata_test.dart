import 'package:quiver_db/quiver_db.dart';
import 'package:quiver_db/src/ffi/bindings.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

void main() {
  // Path to central tests folder
  final testsPath = path.join(
    path.current,
    '..',
    '..',
    'tests',
  );

  group('Scalar Metadata', () {
    test('returns metadata for text attribute', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final metadata = db.getScalarMetadata('Collection', 'label');
        expect(metadata.name, equals('label'));
        expect(metadata.dataType, equals(quiver_data_type_t.QUIVER_DATA_TYPE_STRING));
        expect(metadata.notNull, isTrue);
        expect(metadata.primaryKey, isFalse);
      } finally {
        db.close();
      }
    });

    test('returns metadata for nullable integer attribute', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final metadata = db.getScalarMetadata('Collection', 'some_integer');
        expect(metadata.name, equals('some_integer'));
        expect(metadata.dataType, equals(quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER));
        expect(metadata.notNull, isFalse);
        expect(metadata.primaryKey, isFalse);
      } finally {
        db.close();
      }
    });

    test('returns metadata for nullable real attribute', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final metadata = db.getScalarMetadata('Collection', 'some_float');
        expect(metadata.name, equals('some_float'));
        expect(metadata.dataType, equals(quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT));
        expect(metadata.notNull, isFalse);
        expect(metadata.primaryKey, isFalse);
      } finally {
        db.close();
      }
    });

    test('returns metadata for primary key', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final metadata = db.getScalarMetadata('Collection', 'id');
        expect(metadata.name, equals('id'));
        expect(metadata.dataType, equals(quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER));
        expect(metadata.primaryKey, isTrue);
      } finally {
        db.close();
      }
    });

    test('throws on non-existent collection', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        expect(
          () => db.getScalarMetadata('NonExistent', 'label'),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });

    test('throws on non-existent attribute', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        expect(
          () => db.getScalarMetadata('Collection', 'nonexistent'),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Vector Metadata', () {
    test('returns all attributes in vector group', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final metadata = db.getVectorMetadata('Collection', 'values');
        expect(metadata.groupName, equals('values'));
        expect(metadata.valueColumns.length, equals(2));

        final colNames = metadata.valueColumns.map((a) => a.name).toList();
        expect(colNames, contains('value_int'));
        expect(colNames, contains('value_float'));

        final valueIntCol = metadata.valueColumns.firstWhere((a) => a.name == 'value_int');
        expect(valueIntCol.dataType, equals(quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER));

        final valueFloatCol = metadata.valueColumns.firstWhere((a) => a.name == 'value_float');
        expect(valueFloatCol.dataType, equals(quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT));
      } finally {
        db.close();
      }
    });

    test('throws on non-existent vector group', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        expect(
          () => db.getVectorMetadata('Collection', 'nonexistent'),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Set Metadata', () {
    test('returns all attributes in set group', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final metadata = db.getSetMetadata('Collection', 'tags');
        expect(metadata.groupName, equals('tags'));
        expect(metadata.valueColumns.length, equals(1));

        final tagCol = metadata.valueColumns[0];
        expect(tagCol.name, equals('tag'));
        expect(tagCol.dataType, equals(quiver_data_type_t.QUIVER_DATA_TYPE_STRING));
      } finally {
        db.close();
      }
    });

    test('throws on non-existent set group', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        expect(
          () => db.getSetMetadata('Collection', 'nonexistent'),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Foreign Key Metadata', () {
    test('returns FK info for foreign key attribute', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      try {
        final metadata = db.getScalarMetadata('Child', 'parent_id');
        expect(metadata.isForeignKey, isTrue);
        expect(metadata.referencesCollection, equals('Parent'));
        expect(metadata.referencesColumn, equals('id'));
      } finally {
        db.close();
      }
    });

    test('returns FK info for self-referencing key', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      try {
        final metadata = db.getScalarMetadata('Child', 'sibling_id');
        expect(metadata.isForeignKey, isTrue);
        expect(metadata.referencesCollection, equals('Child'));
        expect(metadata.referencesColumn, equals('id'));
      } finally {
        db.close();
      }
    });

    test('returns no FK info for non-FK attribute', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      try {
        final metadata = db.getScalarMetadata('Child', 'label');
        expect(metadata.isForeignKey, isFalse);
        expect(metadata.referencesCollection, isNull);
        expect(metadata.referencesColumn, isNull);
      } finally {
        db.close();
      }
    });

    test('list_scalar_attributes includes FK info', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      try {
        final attributes = db.listScalarAttributes('Child');
        final parentAttr = attributes.firstWhere((a) => a.name == 'parent_id');
        expect(parentAttr.isForeignKey, isTrue);
        expect(parentAttr.referencesCollection, equals('Parent'));

        final labelAttr = attributes.firstWhere((a) => a.name == 'label');
        expect(labelAttr.isForeignKey, isFalse);
        expect(labelAttr.referencesCollection, isNull);
      } finally {
        db.close();
      }
    });
  });

  group('List Scalar Attributes', () {
    test('returns all scalar attributes with metadata', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final attributes = db.listScalarAttributes('Collection');
        final attrNames = attributes.map((a) => a.name).toList();
        expect(attrNames, contains('id'));
        expect(attrNames, contains('label'));
        expect(attrNames, contains('some_integer'));
        expect(attrNames, contains('some_float'));

        // Verify metadata is included
        final labelAttr = attributes.firstWhere((a) => a.name == 'label');
        expect(labelAttr.dataType, equals(quiver_data_type_t.QUIVER_DATA_TYPE_STRING));
        expect(labelAttr.notNull, isTrue);

        final someIntAttr = attributes.firstWhere((a) => a.name == 'some_integer');
        expect(someIntAttr.dataType, equals(quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER));
        expect(someIntAttr.notNull, isFalse);
      } finally {
        db.close();
      }
    });
  });

  group('List Vector Groups', () {
    test('returns all vector groups with metadata', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final groups = db.listVectorGroups('Collection');
        final groupNames = groups.map((g) => g.groupName).toList();
        expect(groupNames, contains('values'));

        // Verify metadata is included
        final valuesGroup = groups.firstWhere((g) => g.groupName == 'values');
        expect(valuesGroup.valueColumns.length, equals(2));
        final colNames = valuesGroup.valueColumns.map((a) => a.name).toList();
        expect(colNames, contains('value_int'));
        expect(colNames, contains('value_float'));
      } finally {
        db.close();
      }
    });
  });

  group('Read Set Group By ID', () {
    test('reads set group by id', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'all_types.sql'),
      );
      try {
        db.createElement('AllTypes', {'label': 'Item 1'});
        db.updateSetIntegers('AllTypes', 'code', 1, [10, 20, 30]);

        final rows = db.readSetGroupById('AllTypes', 'codes', 1);
        expect(rows, isNotEmpty);
        final codes = rows.map((row) => row['code']).toList()..sort();
        expect(codes, equals([10, 20, 30]));
      } finally {
        db.close();
      }
    });
  });

  group('List Set Groups', () {
    test('returns all set groups with metadata', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final groups = db.listSetGroups('Collection');
        final groupNames = groups.map((g) => g.groupName).toList();
        expect(groupNames, contains('tags'));

        // Verify metadata is included
        final tagsGroup = groups.firstWhere((g) => g.groupName == 'tags');
        expect(tagsGroup.valueColumns.length, equals(1));
        expect(tagsGroup.valueColumns[0].name, equals('tag'));
        expect(tagsGroup.valueColumns[0].dataType, equals(quiver_data_type_t.QUIVER_DATA_TYPE_STRING));
      } finally {
        db.close();
      }
    });
  });
}
