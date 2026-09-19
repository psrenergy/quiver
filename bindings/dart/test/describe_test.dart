import 'package:quiverdb/quiverdb.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

void main() {
  final testsPath = path.join(path.current, '..', '..', 'tests');
  final schemaPath = path.join(testsPath, 'schemas', 'valid', 'collections.sql');

  Database openDb() => Database.fromSchema(':memory:', schemaPath);

  // Shared tests/schemas/ui/ fixture corpus (CORPUS-03) -- never copied into this binding.
  String uiFixture(String name) => path.join(testsPath, 'schemas', 'ui', name);

  Database openUiFixture(String name, String stem) {
    final dir = uiFixture(name);
    return Database.fromSchema(
      path.join(dir, 'dart_$stem.sqlite'),
      path.join(dir, 'schema.sql'),
    );
  }

  group('describe', () {
    test('returns a string', () {
      final db = openDb();
      try {
        expect(db.describe(), isA<String>());
      } finally {
        db.close();
      }
    });
  });

  group('describeCollection', () {
    test('returns a string', () {
      final db = openDb();
      try {
        expect(db.describeCollection('Collection'), isA<String>());
      } finally {
        db.close();
      }
    });
  });

  group('summarizeCollection', () {
    test('returns a string', () {
      final db = openDb();
      try {
        expect(db.summarizeCollection('Collection'), isA<String>());
      } finally {
        db.close();
      }
    });
  });

  // DESC-07: exact-string enum rendering against the shared tests/schemas/ui/ fixtures --
  // a "returns a String" assertion alone cannot catch a per-binding decoding bug (D-31).
  // Dart's `test` package runs files concurrently, so the per-case filename is load-bearing.

  group('enum vocabulary declared with zero elements', () {
    test('renders the declared vocabulary', () {
      final db = openUiFixture('enum_basic', 'declared');
      try {
        final report = db.describeCollection('Storage');
        expect(report, contains('enum bool {0: Disabled, 1: Enabled}'));
      } finally {
        db.close();
      }
    });
  });

  group('enum histogram over twelve elements', () {
    test('renders the value histogram', () {
      final db = openUiFixture('enum_basic', 'histogram');
      try {
        for (var i = 0; i < 8; i++) {
          db.createElement('Storage', {'label': 'Disabled $i', 'has_commitment': 0});
        }
        for (var i = 0; i < 4; i++) {
          db.createElement('Storage', {'label': 'Enabled $i', 'has_commitment': 1});
        }
        final report = db.summarizeCollection('Storage');
        expect(report, contains('values {0: 8 (Disabled), 1: 4 (Enabled)}'));
      } finally {
        db.close();
      }
    });
  });

  group('UI config header line', () {
    test('names the sidecar path and locale', () {
      final db = openUiFixture('enum_basic', 'header');
      try {
        final report = db.describe();
        expect(report, contains('UI config: '));
        expect(report, contains(' (locale: en)'));
      } finally {
        db.close();
      }
    });
  });

  group('unit and hidden decoration', () {
    test('renders unit and hidden markers', () {
      final db = openUiFixture('enum_basic', 'unit_hidden');
      try {
        final report = db.describeCollection('Storage');
        expect(report, contains('[MW]'));
        expect(report, contains('[hidden]'));
      } finally {
        db.close();
      }
    });
  });

  group('accented enum label renders byte-for-byte', () {
    test('renders Seasonal Naïve', () {
      final db = openUiFixture('foresight_like', 'accented');
      try {
        final report = db.describeCollection('EconomicDriver');
        expect(report, contains('Seasonal Naïve'));
      } finally {
        db.close();
      }
    });
  });

  group('no ui/ directory leaves the header off', () {
    test('renders no UI config header', () {
      final db = openUiFixture('no_ui_dir', 'no_sidecar');
      try {
        final report = db.describe();
        expect(report, isNot(contains('UI config: ')));
      } finally {
        db.close();
      }
    });
  });
}
