import 'package:quiver_db/quiver_db.dart';
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
        expect(meta.dataType, equals('text'));
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
        expect(meta.dataType, equals('integer'));
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
        expect(meta.dataType, equals('real'));
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
        expect(meta.dataType, equals('integer'));
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
        expect(meta.attributes.length, equals(2));

        final attrNames = meta.attributes.map((a) => a.name).toList();
        expect(attrNames, contains('value_int'));
        expect(attrNames, contains('value_float'));

        final valueIntAttr = meta.attributes.firstWhere((a) => a.name == 'value_int');
        expect(valueIntAttr.dataType, equals('integer'));

        final valueFloatAttr = meta.attributes.firstWhere((a) => a.name == 'value_float');
        expect(valueFloatAttr.dataType, equals('real'));
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
        expect(meta.attributes.length, equals(1));

        final tagAttr = meta.attributes[0];
        expect(tagAttr.name, equals('tag'));
        expect(tagAttr.dataType, equals('text'));
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
}
