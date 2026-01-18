import 'package:psr_database/psr_database.dart';
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

  group('LuaRunner Create Element', () {
    test('creates element from Lua script', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final lua = LuaRunner(db);
        try {
          lua.run('''
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
          lua.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  group('LuaRunner Read from Lua', () {
    test('reads scalar strings in Lua script', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config'});
        db.createElement('Collection', {'label': 'Item 1', 'some_integer': 10});
        db.createElement('Collection', {'label': 'Item 2', 'some_integer': 20});

        final lua = LuaRunner(db);
        try {
          lua.run('''
            local labels = db:read_scalar_strings("Collection", "label")
            assert(#labels == 2, "Expected 2 labels")
            assert(labels[1] == "Item 1", "First label mismatch")
            assert(labels[2] == "Item 2", "Second label mismatch")
          ''');
        } finally {
          lua.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  group('LuaRunner Script Error', () {
    test('throws LuaException for syntax error', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final lua = LuaRunner(db);
        try {
          expect(
            () => lua.run('invalid syntax !!!'),
            throwsA(isA<LuaException>()),
          );
        } finally {
          lua.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  group('LuaRunner Reuse Runner', () {
    test('runs multiple scripts with same runner', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final lua = LuaRunner(db);
        try {
          lua.run('db:create_element("Configuration", { label = "Config" })');
          lua.run('db:create_element("Collection", { label = "Item 1" })');
          lua.run('db:create_element("Collection", { label = "Item 2" })');

          final labels = db.readScalarStrings('Collection', 'label');
          expect(labels.length, equals(2));
          expect(labels[0], equals('Item 1'));
          expect(labels[1], equals('Item 2'));
        } finally {
          lua.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  // Error handling tests

  group('LuaRunner Undefined Variable', () {
    test('throws on undefined variable access', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final lua = LuaRunner(db);
        try {
          expect(
            () => lua.run('print(undefined_variable.field)'),
            throwsA(isA<LuaException>()),
          );
        } finally {
          lua.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  group('LuaRunner Create Invalid Collection', () {
    test('throws on nonexistent collection in script', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final lua = LuaRunner(db);
        try {
          lua.run('db:create_element("Configuration", { label = "Test Config" })');
          expect(
            () => lua.run('db:create_element("NonexistentCollection", { label = "Item" })'),
            throwsA(isA<LuaException>()),
          );
        } finally {
          lua.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  group('LuaRunner Empty Script', () {
    test('runs empty script successfully', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final lua = LuaRunner(db);
        try {
          // Empty script should succeed without error
          lua.run('');
          // If we get here, the test passed
          expect(true, isTrue);
        } finally {
          lua.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  group('LuaRunner Comment Only Script', () {
    test('runs comment-only script successfully', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final lua = LuaRunner(db);
        try {
          // Comment-only script should succeed
          lua.run('-- this is just a comment');
          expect(true, isTrue);
        } finally {
          lua.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  group('LuaRunner Read Integers', () {
    test('reads scalar integers in Lua script', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config'});
        db.createElement('Collection', {'label': 'Item 1', 'some_integer': 100});
        db.createElement('Collection', {'label': 'Item 2', 'some_integer': 200});

        final lua = LuaRunner(db);
        try {
          lua.run('''
            local ints = db:read_scalar_integers("Collection", "some_integer")
            assert(#ints == 2, "Expected 2 integers")
            assert(ints[1] == 100, "First integer mismatch")
            assert(ints[2] == 200, "Second integer mismatch")
          ''');
        } finally {
          lua.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  group('LuaRunner Read Doubles', () {
    test('reads scalar doubles in Lua script', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config'});
        db.createElement('Collection', {'label': 'Item 1', 'some_float': 1.5});
        db.createElement('Collection', {'label': 'Item 2', 'some_float': 2.5});

        final lua = LuaRunner(db);
        try {
          lua.run('''
            local floats = db:read_scalar_doubles("Collection", "some_float")
            assert(#floats == 2, "Expected 2 doubles")
            assert(floats[1] == 1.5, "First double mismatch")
            assert(floats[2] == 2.5, "Second double mismatch")
          ''');
        } finally {
          lua.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  group('LuaRunner Read Vectors', () {
    test('reads vector integers in Lua script', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config'});
        db.createElement('Collection', {'label': 'Item 1', 'value_int': [1, 2, 3]});

        final lua = LuaRunner(db);
        try {
          lua.run('''
            local vectors = db:read_vector_integers("Collection", "value_int")
            assert(#vectors == 1, "Expected 1 vector")
            assert(#vectors[1] == 3, "Expected 3 elements in vector")
            assert(vectors[1][1] == 1, "First element mismatch")
            assert(vectors[1][2] == 2, "Second element mismatch")
            assert(vectors[1][3] == 3, "Third element mismatch")
          ''');
        } finally {
          lua.dispose();
        }
      } finally {
        db.close();
      }
    });
  });

  group('LuaRunner Create With Vector', () {
    test('creates element with vector in Lua script', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final lua = LuaRunner(db);
        try {
          lua.run('''
            db:create_element("Configuration", { label = "Config" })
            db:create_element("Collection", { label = "Item 1", value_int = {10, 20, 30} })
          ''');

          final result = db.readVectorIntegers('Collection', 'value_int');
          expect(result.length, equals(1));
          expect(result[0], equals([10, 20, 30]));
        } finally {
          lua.dispose();
        }
      } finally {
        db.close();
      }
    });
  });
}
