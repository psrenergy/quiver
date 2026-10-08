import 'dart:convert';

import 'package:quiverdb/quiverdb.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

void main() {
  // Path to central tests folder
  final testsPath = path.join(path.current, '..', '..', 'tests');

  group('Sandbox Create Element', () {
    test('element REAL arrays preserve each Lua cell type', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final sandbox = Sandbox(db);
        try {
          sandbox.run('db:create_element("Collection", { label = "Mixed", value_float = {1, 2.5, true} })');
          expect(
            db.readVectorFloats('Collection', 'value_float'),
            equals([
              [1, 2.5, 1],
            ]),
          );

          sandbox.run('db:update_element_by_label("Collection", "Mixed", { value_float = {false, 3.5, 2} })');
          expect(
            db.readVectorFloats('Collection', 'value_float'),
            equals([
              [0, 3.5, 2],
            ]),
          );
        } finally {
          sandbox.dispose();
        }
      } finally {
        db.close();
      }
    });

    test('creates element from sandbox', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final sandbox = Sandbox(db);
        try {
          sandbox.run('''
            db:create_element("Configuration", { label = "Test Config" })
            db:create_element("Collection", { label = "Item 1", some_integer = 42 })
          ''');

          final labels = db.readScalarStrings('Collection', 'label');
          expect(labels.length, equals(1));
          expect(labels[0], equals('Item 1'));

          final integers = db.readScalarIntegers('Collection', 'some_integer');
          expect(integers.length, equals(1));
          expect(integers[0], equals(42));
        } finally {
          sandbox.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  group('Sandbox Read from sandbox', () {
    test('reads scalar strings in sandbox', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config'});
        db.createElement('Collection', {'label': 'Item 1', 'some_integer': 10});
        db.createElement('Collection', {'label': 'Item 2', 'some_integer': 20});

        final sandbox = Sandbox(db);
        try {
          sandbox.run('''
            local labels = db:read_scalar_strings("Collection", "label")
            assert(#labels == 2, "Expected 2 labels")
            assert(labels[1] == "Item 1", "First label mismatch")
            assert(labels[2] == "Item 2", "Second label mismatch")
          ''');
        } finally {
          sandbox.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  group('Sandbox Script Error', () {
    test('throws SandboxException for syntax error', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final sandbox = Sandbox(db);
        try {
          expect(
            () => sandbox.run('invalid syntax !!!'),
            throwsA(isA<SandboxException>()),
          );
        } finally {
          sandbox.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  group('Sandbox Reuse Runner', () {
    test('runs multiple scripts with same runner', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final sandbox = Sandbox(db);
        try {
          sandbox.run('db:create_element("Configuration", { label = "Config" })');
          sandbox.run('db:create_element("Collection", { label = "Item 1" })');
          sandbox.run('db:create_element("Collection", { label = "Item 2" })');

          final labels = db.readScalarStrings('Collection', 'label');
          expect(labels.length, equals(2));
          expect(labels[0], equals('Item 1'));
          expect(labels[1], equals('Item 2'));
        } finally {
          sandbox.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  // Error handling tests

  group('Sandbox Undefined Variable', () {
    test('throws on undefined variable access', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final sandbox = Sandbox(db);
        try {
          expect(
            () => sandbox.run('print(undefined_variable.field)'),
            throwsA(isA<SandboxException>()),
          );
        } finally {
          sandbox.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  group('Sandbox Create Invalid Collection', () {
    test('throws on nonexistent collection in script', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final sandbox = Sandbox(db);
        try {
          sandbox.run(
            'db:create_element("Configuration", { label = "Test Config" })',
          );
          expect(
            () => sandbox.run(
              'db:create_element("NonexistentCollection", { label = "Item" })',
            ),
            throwsA(isA<SandboxException>()),
          );
        } finally {
          sandbox.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  group('Sandbox Empty Script', () {
    test('runs empty script successfully', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final sandbox = Sandbox(db);
        try {
          // Empty script should succeed without error
          sandbox.run('');
          // If we get here, the test passed
          expect(true, isTrue);
        } finally {
          sandbox.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  group('Sandbox Comment Only Script', () {
    test('runs comment-only script successfully', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final sandbox = Sandbox(db);
        try {
          // Comment-only script should succeed
          sandbox.run('-- this is just a comment');
          expect(true, isTrue);
        } finally {
          sandbox.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  group('Sandbox Read Integers', () {
    test('reads scalar integers in sandbox', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config'});
        db.createElement('Collection', {
          'label': 'Item 1',
          'some_integer': 100,
        });
        db.createElement('Collection', {
          'label': 'Item 2',
          'some_integer': 200,
        });

        final sandbox = Sandbox(db);
        try {
          sandbox.run('''
            local ints = db:read_scalar_integers("Collection", "some_integer")
            assert(#ints == 2, "Expected 2 integers")
            assert(ints[1] == 100, "First integer mismatch")
            assert(ints[2] == 200, "Second integer mismatch")
          ''');
        } finally {
          sandbox.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  group('Sandbox Read Floats', () {
    test('reads scalar floats in sandbox', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config'});
        db.createElement('Collection', {'label': 'Item 1', 'some_float': 1.5});
        db.createElement('Collection', {'label': 'Item 2', 'some_float': 2.5});

        final sandbox = Sandbox(db);
        try {
          sandbox.run('''
            local floats = db:read_scalar_floats("Collection", "some_float")
            assert(#floats == 2, "Expected 2 floats")
            assert(floats[1] == 1.5, "First float mismatch")
            assert(floats[2] == 2.5, "Second float mismatch")
          ''');
        } finally {
          sandbox.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  group('Sandbox Read Vectors', () {
    test('reads vector integers in sandbox', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config'});
        db.createElement('Collection', {
          'label': 'Item 1',
          'value_int': [1, 2, 3],
        });

        final sandbox = Sandbox(db);
        try {
          sandbox.run('''
            local vectors = db:read_vector_integers("Collection", "value_int")
            assert(#vectors == 1, "Expected 1 vector")
            assert(#vectors[1] == 3, "Expected 3 elements in vector")
            assert(vectors[1][1] == 1, "First element mismatch")
            assert(vectors[1][2] == 2, "Second element mismatch")
            assert(vectors[1][3] == 3, "Third element mismatch")
          ''');
        } finally {
          sandbox.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  group('Sandbox Create With Vector', () {
    test('creates element with vector in sandbox', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final sandbox = Sandbox(db);
        try {
          sandbox.run('''
            db:create_element("Configuration", { label = "Config" })
            db:create_element("Collection", { label = "Item 1", value_int = {10, 20, 30} })
          ''');

          final result = db.readVectorIntegers('Collection', 'value_int');
          expect(result.length, equals(1));
          expect(result[0], equals([10, 20, 30]));
        } finally {
          sandbox.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  group('Sandbox return values', () {
    test('returns the script value as JSON', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final sandbox = Sandbox(db);
        try {
          expect(sandbox.run('return { a = 1, b = { 2, 3 } }'), equals('{"a":1,"b":[2,3]}'));
          expect(jsonDecode(sandbox.run('return db:read_element_ids("Collection")')), equals([]));
          // Returning nothing is an empty string, distinct from returning nil.
          expect(sandbox.run('local x = 1'), equals(''));
          expect(sandbox.run('return nil'), equals('null'));
        } finally {
          sandbox.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  group('Database dry run', () {
    test('rolls back a script', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final sandbox = Sandbox(db);
        try {
          sandbox.run('db:create_element("Configuration", { label = "Config" })');

          expect(db.inDryRun(), isFalse);
          final preview = db.dryRun((db) {
            expect(db.inDryRun(), isTrue);
            // db:transaction composes: the dry run absorbs the nested BEGIN/COMMIT.
            return sandbox.run('''
              db:transaction(function(db)
                db:create_element("Collection", { label = "Preview" })
              end)
              return db:read_scalar_strings("Collection", "label")
            ''');
          });

          expect(jsonDecode(preview), equals(['Preview']));
          expect(db.inDryRun(), isFalse);
          expect(db.readScalarStrings('Collection', 'label'), isEmpty);
        } finally {
          sandbox.dispose();
        }
      } finally {
        db.close();
      }
    });

    test('endDryRun without a dry run throws', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        expect(() => db.endDryRun(), throwsA(isA<DatabaseException>()));
      } finally {
        db.close();
      }
    });
  });
}
