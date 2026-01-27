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

  group('Time Series', () {
    test('add and read single row', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'time_series.sql'),
      );
      try {
        final id = db.createElement('Resource', {'label': 'Resource 1'});
        expect(id, equals(1));

        // Add a time series row
        db.addTimeSeriesRow('Resource', 'value1', id, 42.5, '2024-01-01T00:00:00');

        // Read back
        final rows = db.readTimeSeriesTable('Resource', 'group1', id);
        expect(rows.length, equals(1));
        expect(rows[0]['date_time'], equals('2024-01-01T00:00:00'));
        expect(rows[0]['value1'], equals(42.5));
      } finally {
        db.close();
      }
    });

    test('add multiple rows', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'time_series.sql'),
      );
      try {
        final id = db.createElement('Resource', {'label': 'Resource 1'});

        // Add multiple rows
        db.addTimeSeriesRow('Resource', 'value1', id, 10.0, '2024-01-01T00:00:00');
        db.addTimeSeriesRow('Resource', 'value1', id, 20.0, '2024-01-02T00:00:00');
        db.addTimeSeriesRow('Resource', 'value1', id, 30.0, '2024-01-03T00:00:00');

        final rows = db.readTimeSeriesTable('Resource', 'group1', id);
        expect(rows.length, equals(3));

        // Should be ordered by date_time
        expect(rows[0]['date_time'], equals('2024-01-01T00:00:00'));
        expect(rows[1]['date_time'], equals('2024-01-02T00:00:00'));
        expect(rows[2]['date_time'], equals('2024-01-03T00:00:00'));

        expect(rows[0]['value1'], equals(10.0));
        expect(rows[1]['value1'], equals(20.0));
        expect(rows[2]['value1'], equals(30.0));
      } finally {
        db.close();
      }
    });

    test('multi-dimensional with block', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'time_series.sql'),
      );
      try {
        final id = db.createElement('Resource', {'label': 'Resource 1'});

        // Add rows with block dimension
        db.addTimeSeriesRow('Resource', 'value3', id, 100.0, '2024-01-01T00:00:00', dimensions: {'block': 1});
        db.addTimeSeriesRow('Resource', 'value3', id, 200.0, '2024-01-01T00:00:00', dimensions: {'block': 2});
        db.addTimeSeriesRow('Resource', 'value3', id, 150.0, '2024-01-02T00:00:00', dimensions: {'block': 1});

        final rows = db.readTimeSeriesTable('Resource', 'group2', id);
        expect(rows.length, equals(3));

        // Verify first row
        expect(rows[0]['date_time'], equals('2024-01-01T00:00:00'));
        expect(rows[0]['block'], equals(1));
        expect(rows[0]['value3'], equals(100.0));
      } finally {
        db.close();
      }
    });

    test('update existing row', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'time_series.sql'),
      );
      try {
        final id = db.createElement('Resource', {'label': 'Resource 1'});

        // Add initial row
        db.addTimeSeriesRow('Resource', 'value1', id, 42.5, '2024-01-01T00:00:00');

        // Update it
        db.updateTimeSeriesRow('Resource', 'value1', id, 99.9, '2024-01-01T00:00:00');

        final rows = db.readTimeSeriesTable('Resource', 'group1', id);
        expect(rows.length, equals(1));
        expect(rows[0]['value1'], equals(99.9));
      } finally {
        db.close();
      }
    });

    test('delete all for element', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'time_series.sql'),
      );
      try {
        final id = db.createElement('Resource', {'label': 'Resource 1'});

        // Add multiple rows
        db.addTimeSeriesRow('Resource', 'value1', id, 10.0, '2024-01-01T00:00:00');
        db.addTimeSeriesRow('Resource', 'value1', id, 20.0, '2024-01-02T00:00:00');

        // Delete all
        db.deleteTimeSeries('Resource', 'group1', id);

        final rows = db.readTimeSeriesTable('Resource', 'group1', id);
        expect(rows, isEmpty);
      } finally {
        db.close();
      }
    });

    test('get metadata', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'time_series.sql'),
      );
      try {
        final meta = db.getTimeSeriesMetadata('Resource', 'group1');
        expect(meta.groupName, equals('group1'));
        expect(meta.dimensionColumns.length, equals(1));
        expect(meta.dimensionColumns.contains('date_time'), isTrue);
        expect(meta.valueColumns.length, equals(2)); // value1 and value2
      } finally {
        db.close();
      }
    });

    test('list time series groups', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'time_series.sql'),
      );
      try {
        final groups = db.listTimeSeriesGroups('Resource');
        expect(groups.length, equals(3)); // group1, group2, group3
      } finally {
        db.close();
      }
    });
  });
}
