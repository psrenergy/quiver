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
        final meta = db.getScalarMetadata('Collection', 'label');
        expect(meta.name, equals('label'));
        expect(meta.dataType, equals(quiver_data_type_t.QUIVER_DATA_TYPE_STRING));
        expect(meta.notNull, isTrue);
        expect(meta.primaryKey, isFalse);
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
        final meta = db.getScalarMetadata('Collection', 'some_integer');
        expect(meta.name, equals('some_integer'));
        expect(meta.dataType, equals(quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER));
        expect(meta.notNull, isFalse);
        expect(meta.primaryKey, isFalse);
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
        final meta = db.getScalarMetadata('Collection', 'some_float');
        expect(meta.name, equals('some_float'));
        expect(meta.dataType, equals(quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT));
        expect(meta.notNull, isFalse);
        expect(meta.primaryKey, isFalse);
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
        final meta = db.getScalarMetadata('Collection', 'id');
        expect(meta.name, equals('id'));
        expect(meta.dataType, equals(quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER));
        expect(meta.primaryKey, isTrue);
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
        final meta = db.getVectorMetadata('Collection', 'values');
        expect(meta.groupName, equals('values'));
        expect(meta.valueColumns.length, equals(2));

        final colNames = meta.valueColumns.map((a) => a.name).toList();
        expect(colNames, contains('value_int'));
        expect(colNames, contains('value_float'));

        final valueIntCol = meta.valueColumns.firstWhere((a) => a.name == 'value_int');
        expect(valueIntCol.dataType, equals(quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER));

        final valueFloatCol = meta.valueColumns.firstWhere((a) => a.name == 'value_float');
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
        final meta = db.getSetMetadata('Collection', 'tags');
        expect(meta.groupName, equals('tags'));
        expect(meta.valueColumns.length, equals(1));

        final tagCol = meta.valueColumns[0];
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
        final meta = db.getScalarMetadata('Child', 'parent_id');
        expect(meta.isForeignKey, isTrue);
        expect(meta.referencesCollection, equals('Parent'));
        expect(meta.referencesColumn, equals('id'));
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
        final meta = db.getScalarMetadata('Child', 'sibling_id');
        expect(meta.isForeignKey, isTrue);
        expect(meta.referencesCollection, equals('Child'));
        expect(meta.referencesColumn, equals('id'));
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
        final meta = db.getScalarMetadata('Child', 'label');
        expect(meta.isForeignKey, isFalse);
        expect(meta.referencesCollection, isNull);
        expect(meta.referencesColumn, isNull);
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
        final attrs = db.listScalarAttributes('Child');
        final parentAttr = attrs.firstWhere((a) => a.name == 'parent_id');
        expect(parentAttr.isForeignKey, isTrue);
        expect(parentAttr.referencesCollection, equals('Parent'));

        final labelAttr = attrs.firstWhere((a) => a.name == 'label');
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
        final attrs = db.listScalarAttributes('Collection');
        final attrNames = attrs.map((a) => a.name).toList();
        expect(attrNames, contains('id'));
        expect(attrNames, contains('label'));
        expect(attrNames, contains('some_integer'));
        expect(attrNames, contains('some_float'));

        // Verify metadata is included
        final labelAttr = attrs.firstWhere((a) => a.name == 'label');
        expect(labelAttr.dataType, equals(quiver_data_type_t.QUIVER_DATA_TYPE_STRING));
        expect(labelAttr.notNull, isTrue);

        final someIntAttr = attrs.firstWhere((a) => a.name == 'some_integer');
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
