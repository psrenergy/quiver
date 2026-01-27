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

  group('Set and Read Scalar Relations', () {
    test('set and read scalar relation', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});

        // Create parents
        db.createElement('Parent', {'label': 'Parent 1'});
        db.createElement('Parent', {'label': 'Parent 2'});

        // Create children
        db.createElement('Child', {'label': 'Child 1'});
        db.createElement('Child', {'label': 'Child 2'});
        db.createElement('Child', {'label': 'Child 3'});

        // Set relations
        db.setScalarRelation('Child', 'parent_id', 'Child 1', 'Parent 1');
        db.setScalarRelation('Child', 'parent_id', 'Child 3', 'Parent 2');

        // Read relations
        final labels = db.readScalarRelation('Child', 'parent_id');
        expect(labels.length, equals(3));
        expect(labels[0], equals('Parent 1'));
        expect(labels[1], isNull); // Child 2 has no parent
        expect(labels[2], equals('Parent 2'));
      } finally {
        db.close();
      }
    });
  });

  group('Scalar Relations Self-Reference', () {
    test('read scalar relation self-reference', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});

        // Create children (Child references itself via sibling_id)
        db.createElement('Child', {'label': 'Child 1'});
        db.createElement('Child', {'label': 'Child 2'});

        // Set sibling relation (self-reference)
        db.setScalarRelation('Child', 'sibling_id', 'Child 1', 'Child 2');

        // Read sibling relations
        final labels = db.readScalarRelation('Child', 'sibling_id');
        expect(labels.length, equals(2));
        expect(labels[0], equals('Child 2')); // Child 1's sibling is Child 2
        expect(labels[1], isNull); // Child 2 has no sibling set
      } finally {
        db.close();
      }
    });
  });

  group('Scalar Relations Empty Result', () {
    test('read scalar relation empty result', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});

        // No Child elements created
        final labels = db.readScalarRelation('Child', 'parent_id');
        expect(labels, isEmpty);
      } finally {
        db.close();
      }
    });
  });

  group('Scalar Relation Invalid Collection', () {
    test('throws on nonexistent collection', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        expect(
          () => db.readScalarRelation('NonexistentCollection', 'parent_id'),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });
}
