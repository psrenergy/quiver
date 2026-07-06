import 'package:quiverdb/quiverdb.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

void main() {
  // Path to central tests folder
  final testsPath = path.join(path.current, '..', '..', 'tests');

  group('Time Series NULL cells', () {
    // nullable_time_series.sql: Sensor.readings with date_time TEXT NOT NULL,
    // temperature REAL, counter INTEGER, status TEXT (nullable value columns).
    Database openNullable() => Database.fromSchema(
      ':memory:',
      path.join(testsPath, 'schemas', 'valid', 'nullable_time_series.sql'),
    );

    test('null cells round-trip through update and read', () {
      final db = openNullable();
      try {
        final id = db.createElement('Sensor', {'label': 'S1'});
        db.updateTimeSeriesGroup('Sensor', 'readings', id, {
          'date_time': ['2024-01-01', '2024-01-02'],
          'temperature': [10.5, null],
          'counter': [null, 7],
          'status': ['ok', null],
        });

        final result = db.readTimeSeriesGroup('Sensor', 'readings', id);
        expect(result['temperature'], equals([10.5, null]));
        expect(result['counter'], equals([null, 7]));
        expect(result['status'], equals(['ok', null]));
      } finally {
        db.close();
      }
    });

    test('all-null value column reads back as all nulls', () {
      final db = openNullable();
      try {
        final id = db.createElement('Sensor', {'label': 'S1'});
        db.updateTimeSeriesGroup('Sensor', 'readings', id, {
          'date_time': ['2024-01-01', '2024-01-02'],
          'status': [null, null],
        });

        final result = db.readTimeSeriesGroup('Sensor', 'readings', id);
        expect(result['status'], equals([null, null]));
      } finally {
        db.close();
      }
    });

    test('null cells survive a read-modify-write round-trip', () {
      final db = openNullable();
      try {
        final id = db.createElement('Sensor', {'label': 'S1'});
        db.updateTimeSeriesGroup('Sensor', 'readings', id, {
          'date_time': ['2024-01-01', '2024-01-02'],
          'temperature': [10.5, null],
        });

        final ts = db.readTimeSeriesGroup('Sensor', 'readings', id);
        ts['temperature']![0] = 99.0;
        db.updateTimeSeriesGroup('Sensor', 'readings', id, ts);

        final result = db.readTimeSeriesGroup('Sensor', 'readings', id);
        expect(result['temperature'], equals([99.0, null]));
      } finally {
        db.close();
      }
    });
  });
}
