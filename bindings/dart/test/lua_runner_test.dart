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
}
