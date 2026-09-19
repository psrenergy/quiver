import 'dart:ffi';
import 'dart:io';

import 'package:quiverdb/quiverdb.dart';
import 'package:quiverdb/src/ffi/bindings.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

// OPT-01/OPT-02/OPT-03/OPT-04/SAFE-01 (Phase 2, plan 02-04): the hand-edited 24-byte options
// struct, the two new named open() parameters, and hasUiConfig(), proven end-to-end against the
// shared tests/schemas/ui/foresight_like fixture -- never copied into this binding.
//
// The database file is placed in a fresh temp directory with NO `ui/` sibling of its own, which
// is what makes the explicit `uiConfigDir` proof real: the convention path (`<db_dir>/ui/`)
// cannot resolve anything there, so a rendered label can only have come from the explicit
// override.

void main() {
  final testsPath = path.join(path.current, '..', '..', 'tests');
  final schemaPath = path.join(testsPath, 'schemas', 'ui', 'foresight_like', 'schema.sql');
  final uiConfigDir = path.join(testsPath, 'schemas', 'ui', 'foresight_like', 'ui');
  final malformedUiDir = path.join(testsPath, 'schemas', 'ui', 'malformed', 'ui');

  Directory freshScratchDir() => Directory.systemTemp.createTempSync('quiver_dart_ui_options_');

  Database openScratch({String? uiConfigDirArg, String? uiLocale}) {
    final dir = freshScratchDir();
    addTearDown(() => dir.deleteSync(recursive: true));
    return Database.fromSchema(
      path.join(dir.path, 'db.sqlite'),
      schemaPath,
      uiConfigDir: uiConfigDirArg,
      uiLocale: uiLocale,
    );
  }

  // Gap 7 (02-VERIFICATION.md): from_migrations() needs a migrations directory that creates the
  // EconomicDriver schema the foresight_like sidecar labels -- no such fixture is checked in (the
  // plan's files_modified list is the six test files only), so this writes one at runtime into a
  // scratch directory, mirroring foresight_like/schema.sql's own DDL. Nothing here is committed.
  String scratchMigrationsDir() {
    final dir = freshScratchDir();
    final upDir = Directory(path.join(dir.path, '1'))..createSync(recursive: true);
    File(path.join(upDir.path, 'up.sql')).writeAsStringSync(File(schemaPath).readAsStringSync());
    File(
      path.join(upDir.path, 'down.sql'),
    ).writeAsStringSync('DROP TABLE EconomicDriver;\nDROP TABLE Configuration;\n');
    return dir.path;
  }

  group('options struct is 24 bytes', () {
    test('sizeOf<quiver_database_options_t>() is 24', () {
      expect(sizeOf<quiver_database_options_t>(), equals(24));
    });
  });

  group('explicit uiConfigDir loads a config not beside the database', () {
    test('reads the sidecar from a directory the database is not in', () {
      final db = openScratch(uiConfigDirArg: uiConfigDir);
      try {
        expect(db.hasUiConfig(), isTrue);
        final report = db.describeCollection('EconomicDriver');
        expect(report, contains('Seasonal Naïve'));
      } finally {
        db.close();
      }
    });
  });

  group('spanish locale renders spanish labels', () {
    test('renders Ingenuo Estacional and Tendencia Lineal Local, not the English forms', () {
      final db = openScratch(uiConfigDirArg: uiConfigDir, uiLocale: 'es');
      try {
        final report = db.describeCollection('EconomicDriver');
        expect(report, contains('Ingenuo Estacional'));
        expect(report, contains('Tendencia Lineal Local'));
        expect(report, isNot(contains('Seasonal Naïve')));
        expect(report, isNot(contains('Local Linear Trend')));
      } finally {
        db.close();
      }
    });
  });

  group('default locale renders english labels', () {
    test('renders Seasonal Naïve and Local Linear Trend, not the Spanish forms', () {
      final db = openScratch(uiConfigDirArg: uiConfigDir);
      try {
        final report = db.describeCollection('EconomicDriver');
        expect(report, contains('Seasonal Naïve'));
        expect(report, contains('Local Linear Trend'));
        expect(report, isNot(contains('Ingenuo Estacional')));
        expect(report, isNot(contains('Tendencia Lineal Local')));
      } finally {
        db.close();
      }
    });
  });

  group('missing uiConfigDir degrades without throwing', () {
    test('hasUiConfig() is false, no exception, no convention sidecar in an empty temp dir', () {
      final db = openScratch();
      try {
        expect(db.hasUiConfig(), isFalse);
        final report = db.describeCollection('EconomicDriver');
        expect(report, isA<String>());
      } finally {
        db.close();
      }
    });
  });

  // Gap 5 (02-VERIFICATION.md): malformed polarity, previously C++-only.
  group('malformed sidecar', () {
    test('reports false without throwing', () {
      final db = openScratch(uiConfigDirArg: malformedUiDir);
      try {
        expect(db.hasUiConfig(), isFalse);
      } finally {
        db.close();
      }
    });
  });

  // Gap 5 (02-VERIFICATION.md): the D-01 :memory: distinction, previously C++-only.
  group(':memory: distinction', () {
    test('explicit uiConfigDir loads even for :memory:, convention path does not', () {
      final withDir = Database.fromSchema(
        ':memory:',
        schemaPath,
        uiConfigDir: uiConfigDir,
      );
      try {
        expect(withDir.hasUiConfig(), isTrue);
      } finally {
        withDir.close();
      }

      final withoutDir = Database.fromSchema(':memory:', schemaPath);
      try {
        expect(withoutDir.hasUiConfig(), isFalse);
      } finally {
        withoutDir.close();
      }
    });
  });

  // Gap 7 (02-VERIFICATION.md): open()/fromMigrations() with uiConfigDir + uiLocale, previously
  // proven only through fromSchema() by a committed test.
  group('open() threads uiConfigDir and uiLocale', () {
    test('renders the Spanish label after reopening', () {
      final dir = freshScratchDir();
      addTearDown(() => dir.deleteSync(recursive: true));
      final dbPath = path.join(dir.path, 'db.sqlite');
      Database.fromSchema(dbPath, schemaPath).close();

      final db = Database.open(dbPath, uiConfigDir: uiConfigDir, uiLocale: 'es');
      try {
        final report = db.describeCollection('EconomicDriver');
        expect(report, contains('Ingenuo Estacional'));
        expect(report, contains('Tendencia Lineal Local'));
        expect(report, isNot(contains('Seasonal Naïve')));
      } finally {
        db.close();
      }
    });
  });

  group('fromMigrations() threads uiConfigDir and uiLocale', () {
    test('renders the Spanish label on a migrated database', () {
      final migrationsDir = scratchMigrationsDir();
      final dbDir = freshScratchDir();
      addTearDown(() => dbDir.deleteSync(recursive: true));
      final db = Database.fromMigrations(
        path.join(dbDir.path, 'db.sqlite'),
        migrationsDir,
        uiConfigDir: uiConfigDir,
        uiLocale: 'es',
      );
      try {
        final report = db.describeCollection('EconomicDriver');
        expect(report, contains('Ingenuo Estacional'));
        expect(report, contains('Tendencia Lineal Local'));
        expect(report, isNot(contains('Seasonal Naïve')));
      } finally {
        db.close();
      }
    });
  });

  // Gap 7 (02-VERIFICATION.md): a locale label through whole-database describe(), not only
  // describeCollection().
  group('describe() carries the locale-specific label', () {
    test('whole-database describe() contains the Spanish label', () {
      final db = openScratch(uiConfigDirArg: uiConfigDir, uiLocale: 'es');
      try {
        final report = db.describe();
        expect(report, contains('Ingenuo Estacional'));
        expect(report, isNot(contains('Seasonal Naïve')));
      } finally {
        db.close();
      }
    });
  });

  // OPT-03/empty (02-09 edge lift): an empty-string uiLocale is unset (D-03), not a locale named
  // "".
  group('empty-string uiLocale', () {
    test('matches the unset/en output', () {
      final db = openScratch(uiConfigDirArg: uiConfigDir, uiLocale: '');
      try {
        final report = db.describeCollection('EconomicDriver');
        expect(report, contains('Seasonal Naïve'));
        expect(report, isNot(contains('Ingenuo Estacional')));
      } finally {
        db.close();
      }
    });
  });
}
