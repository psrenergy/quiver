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

  group('Time Series Metadata', () {
    test('getTimeSeriesMetadata returns metadata for group', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final meta = db.getTimeSeriesMetadata('Collection', 'data');
        expect(meta.groupName, equals('data'));
        expect(meta.valueColumns.length, equals(1));
        expect(meta.valueColumns[0].name, equals('value'));
      } finally {
        db.close();
      }
    });

    test('listTimeSeriesGroups returns all groups for collection', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final groups = db.listTimeSeriesGroups('Collection');
        expect(groups.length, equals(1));
        expect(groups[0].groupName, equals('data'));
      } finally {
        db.close();
      }
    });

    test('listTimeSeriesGroups returns empty for collection without time series', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        final groups = db.listTimeSeriesGroups('Configuration');
        expect(groups, isEmpty);
      } finally {
        db.close();
      }
    });
  });

  group('Time Series Read', () {
    test('readTimeSeriesGroupById returns ordered rows', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Collection', {'label': 'Item 1'});

        // Insert time series data
        db.updateTimeSeriesGroup('Collection', 'data', id, [
          {'date_time': '2024-01-01T10:00:00', 'value': 1.5},
          {'date_time': '2024-01-01T11:00:00', 'value': 2.5},
          {'date_time': '2024-01-01T12:00:00', 'value': 3.5},
        ]);

        final rows = db.readTimeSeriesGroupById('Collection', 'data', id);
        expect(rows.length, equals(3));
        expect(rows[0]['date_time'], equals('2024-01-01T10:00:00'));
        expect(rows[0]['value'], equals(1.5));
        expect(rows[1]['date_time'], equals('2024-01-01T11:00:00'));
        expect(rows[1]['value'], equals(2.5));
        expect(rows[2]['date_time'], equals('2024-01-01T12:00:00'));
        expect(rows[2]['value'], equals(3.5));
      } finally {
        db.close();
      }
    });

    test('readTimeSeriesGroupById returns empty for no data', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Collection', {'label': 'Item 1'});

        final rows = db.readTimeSeriesGroupById('Collection', 'data', id);
        expect(rows, isEmpty);
      } finally {
        db.close();
      }
    });
  });

  group('Time Series Update', () {
    test('updateTimeSeriesGroup replaces all rows', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Collection', {'label': 'Item 1'});

        // Insert initial data
        db.updateTimeSeriesGroup('Collection', 'data', id, [
          {'date_time': '2024-01-01T10:00:00', 'value': 1.0},
        ]);

        var rows = db.readTimeSeriesGroupById('Collection', 'data', id);
        expect(rows.length, equals(1));

        // Replace with new data
        db.updateTimeSeriesGroup('Collection', 'data', id, [
          {'date_time': '2024-02-01T10:00:00', 'value': 10.0},
          {'date_time': '2024-02-01T11:00:00', 'value': 20.0},
        ]);

        rows = db.readTimeSeriesGroupById('Collection', 'data', id);
        expect(rows.length, equals(2));
        expect(rows[0]['date_time'], equals('2024-02-01T10:00:00'));
        expect(rows[0]['value'], equals(10.0));
      } finally {
        db.close();
      }
    });

    test('updateTimeSeriesGroup clears data with empty list', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Collection', {'label': 'Item 1'});

        // Insert data
        db.updateTimeSeriesGroup('Collection', 'data', id, [
          {'date_time': '2024-01-01T10:00:00', 'value': 1.0},
        ]);

        // Clear
        db.updateTimeSeriesGroup('Collection', 'data', id, []);

        final rows = db.readTimeSeriesGroupById('Collection', 'data', id);
        expect(rows, isEmpty);
      } finally {
        db.close();
      }
    });

    test('time series data is ordered by date_time', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Collection', {'label': 'Item 1'});

        // Insert out of order
        db.updateTimeSeriesGroup('Collection', 'data', id, [
          {'date_time': '2024-01-03T10:00:00', 'value': 3.0},
          {'date_time': '2024-01-01T10:00:00', 'value': 1.0},
          {'date_time': '2024-01-02T10:00:00', 'value': 2.0},
        ]);

        final rows = db.readTimeSeriesGroupById('Collection', 'data', id);
        expect(rows.length, equals(3));
        // Should be ordered by date_time
        expect(rows[0]['date_time'], equals('2024-01-01T10:00:00'));
        expect(rows[1]['date_time'], equals('2024-01-02T10:00:00'));
        expect(rows[2]['date_time'], equals('2024-01-03T10:00:00'));
      } finally {
        db.close();
      }
    });
  });

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
          throwsA(isA<QuiverException>()),
        );
      } finally {
        db.close();
      }
    });

    test('listTimeSeriesFilesColumns throws for collection without files table', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        expect(
          () => db.listTimeSeriesFilesColumns('Configuration'),
          throwsA(isA<QuiverException>()),
        );
      } finally {
        db.close();
      }
    });
  });
}
