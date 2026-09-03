import 'package:quiverdb/quiverdb.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

void main() {
  // Path to central tests folder
  final testsPath = path.join(path.current, '..', '..', 'tests');

  group('Add Time Series Row', () {
    test('insert single row and read it back', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Collection', {'label': 'Item 1'});

        db.upsertTimeSeriesRow('Collection', 'data', id, {
          'date_time': '2024-01-01T10:00:00',
          'value': 10.0,
        });

        final result = db.readTimeSeriesGroup('Collection', 'data', id);
        expect(result['date_time']!.length, equals(1));
        expect(result['date_time']![0], equals(DateTime(2024, 1, 1, 10, 0, 0)));
        expect(result['value']![0], equals(10.0));
      } finally {
        db.close();
      }
    });

    test('upsert overwrites value columns for same dimension PK', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Collection', {'label': 'Item 1'});

        // Initial insert
        db.upsertTimeSeriesRow('Collection', 'data', id, {
          'date_time': '2024-01-01T10:00:00',
          'value': 10.0,
        });

        // Upsert same date_time with new value
        db.upsertTimeSeriesRow('Collection', 'data', id, {
          'date_time': '2024-01-01T10:00:00',
          'value': 99.0,
        });

        final result = db.readTimeSeriesGroup('Collection', 'data', id);
        expect(result['date_time']!.length, equals(1));
        expect(result['value']![0], equals(99.0));
      } finally {
        db.close();
      }
    });

    test('multi-dim happy path with date_time and block composite PK', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'multi_dim_time_series.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Resource', {'label': 'Resource 1'});

        db.upsertTimeSeriesRow('Resource', 'load', id, {
          'date_time': '2024-01-01T00:00:00',
          'block': 1,
          'load': 42.5,
          'flag': 0,
        });

        final result = db.readTimeSeriesGroup('Resource', 'load', id);
        expect(result['date_time']!.length, equals(1));
        expect(result['date_time']![0], equals(DateTime(2024, 1, 1, 0, 0, 0)));
        expect(result['block']![0], equals(1));
        expect(result['load']![0], equals(42.5));
        expect(result['flag']![0], equals(0));
      } finally {
        db.close();
      }
    });

    test('upsertTimeSeriesRowByLabel writes only the labelled element', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final item = db.createElement('Collection', {'label': 'Item 1'});
        final other = db.createElement('Collection', {'label': 'Item 2'});

        db.upsertTimeSeriesRowByLabel('Collection', 'data', 'Item 2', {
          'date_time': '2024-01-01T10:00:00',
          'value': 99.0,
        });
        db.upsertTimeSeriesRowByLabel('Collection', 'data', 'Item 1', {
          'date_time': '2024-01-01T10:00:00',
          'value': 10.0,
        });
        // Same dimension PK upserts rather than appending.
        db.upsertTimeSeriesRowByLabel('Collection', 'data', 'Item 1', {
          'date_time': '2024-01-01T10:00:00',
          'value': 20.0,
        });

        final result = db.readTimeSeriesGroup('Collection', 'data', item);
        expect(result['date_time']!.length, equals(1));
        expect(result['value']![0], equals(20.0));
        expect(
          db.readTimeSeriesGroup('Collection', 'data', other)['value'],
          equals([99.0]),
        );
      } finally {
        db.close();
      }
    });

    test('upsertTimeSeriesRowByLabel throws on an unresolvable label', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Collection', {'label': 'Item 1'});

        expect(
          () => db.upsertTimeSeriesRowByLabel('Collection', 'data', 'Nope', {
            'date_time': '2024-01-01T10:00:00',
            'value': 10.0,
          }),
          throwsA(isA<DatabaseException>()),
        );
        expect(db.readTimeSeriesGroup('Collection', 'data', id), isEmpty);
      } finally {
        db.close();
      }
    });
  });

  group('Read Time Series Row', () {
    test('readTimeSeriesRow returns one value per element', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final id1 = db.createElement('Collection', {'label': 'Item 1'});
        final id2 = db.createElement('Collection', {'label': 'Item 2'});

        db.updateTimeSeriesGroup('Collection', 'data', id1, {
          'date_time': ['2024-01-01T00:00:00', '2024-02-01T00:00:00'],
          'value': [10.5, 20.5],
        });
        db.updateTimeSeriesGroup('Collection', 'data', id2, {
          'date_time': ['2024-01-01T00:00:00'],
          'value': [30.5],
        });

        final row = db.readTimeSeriesRow(
          'Collection',
          'data',
          'value',
          DateTime(2024, 1, 15),
        );
        expect(row, equals([10.5, 30.5]));
      } finally {
        db.close();
      }
    });

    test('readTimeSeriesRow returns empty list without elements', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        final row = db.readTimeSeriesRow(
          'Collection',
          'data',
          'value',
          DateTime(2024, 1, 15),
        );
        expect(row, isEmpty);
      } finally {
        db.close();
      }
    });

    test('readTimeSeriesRow yields null for elements with no data', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'nullable_time_series.sql'),
      );
      try {
        final id1 = db.createElement('Sensor', {'label': 'Sensor 1'});
        db.createElement('Sensor', {'label': 'Sensor 2'}); // no data

        db.updateTimeSeriesGroup('Sensor', 'readings', id1, {
          'date_time': ['2024-01-02T00:00:00'],
          'temperature': [10.5],
          'counter': [0],
        });

        // Before any row: absence, never a NaN or 0 sentinel
        expect(
          db.readTimeSeriesRow(
            'Sensor',
            'readings',
            'temperature',
            DateTime(2024, 1, 1),
          ),
          equals([null, null]),
        );
        expect(
          db.readTimeSeriesRow(
            'Sensor',
            'readings',
            'counter',
            DateTime(2024, 1, 2),
          ),
          equals([0, null]),
        );
      } finally {
        db.close();
      }
    });
  });
}
