import 'package:quiverdb/quiverdb.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

void main() {
  // Path to central tests folder
  final testsPath = path.join(path.current, '..', '..', 'tests');

  group('Time Series Files', () {
    test('hasTimeSeriesFiles returns true for collection with files table', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        expect(db.hasTimeSeriesFiles('Collection'), isTrue);
        expect(db.hasTimeSeriesFiles('Configuration'), isFalse);
      } finally {
        db.close();
      }
    });

    test('listTimeSeriesFilesColumns returns column names', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final columns = db.listTimeSeriesFilesColumns('Collection');
        expect(columns.length, equals(2));
        expect(columns, contains('data_file'));
        expect(columns, contains('metadata_file'));
      } finally {
        db.close();
      }
    });

    test('readTimeSeriesFiles returns nulls for empty table', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final paths = db.readTimeSeriesFiles('Collection');
        expect(paths.length, equals(2));
        expect(paths['data_file'], isNull);
        expect(paths['metadata_file'], isNull);
      } finally {
        db.close();
      }
    });

    test('updateTimeSeriesFiles and readTimeSeriesFiles roundtrip', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.updateTimeSeriesFiles('Collection', {
          'data_file': '/path/to/data.csv',
          'metadata_file': '/path/to/meta.json',
        });

        final paths = db.readTimeSeriesFiles('Collection');
        expect(paths['data_file'], equals('/path/to/data.csv'));
        expect(paths['metadata_file'], equals('/path/to/meta.json'));
      } finally {
        db.close();
      }
    });

    test('updateTimeSeriesFiles supports null values', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.updateTimeSeriesFiles('Collection', {
          'data_file': '/path/to/data.csv',
          'metadata_file': null,
        });

        final paths = db.readTimeSeriesFiles('Collection');
        expect(paths['data_file'], equals('/path/to/data.csv'));
        expect(paths['metadata_file'], isNull);
      } finally {
        db.close();
      }
    });

    test('updateTimeSeriesFiles replaces existing values', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        // First update
        db.updateTimeSeriesFiles('Collection', {
          'data_file': '/old/data.csv',
          'metadata_file': '/old/meta.json',
        });

        // Second update replaces
        db.updateTimeSeriesFiles('Collection', {
          'data_file': '/new/data.csv',
          'metadata_file': '/new/meta.json',
        });

        final paths = db.readTimeSeriesFiles('Collection');
        expect(paths['data_file'], equals('/new/data.csv'));
        expect(paths['metadata_file'], equals('/new/meta.json'));
      } finally {
        db.close();
      }
    });

    test('readTimeSeriesFiles throws for collection without files table', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        expect(
          () => db.readTimeSeriesFiles('Configuration'),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });

    test(
      'listTimeSeriesFilesColumns throws for collection without files table',
      () {
        final db = Database.fromSchema(
          ':memory:',
          path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
        );
        try {
          expect(
            () => db.listTimeSeriesFilesColumns('Configuration'),
            throwsA(isA<DatabaseException>()),
          );
        } finally {
          db.close();
        }
      },
    );
  });
}
