import 'package:quiverdb/quiverdb.dart';
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
        final metadata = db.getTimeSeriesMetadata('Collection', 'data');
        expect(metadata.groupName, equals('data'));
        expect(metadata.dimensionColumn, equals('date_time'));
        expect(metadata.valueColumns.length, equals(1));
        expect(metadata.valueColumns[0].name, equals('value'));
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
        expect(groups[0].dimensionColumn, equals('date_time'));
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
    test('readTimeSeriesGroup returns ordered rows', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Collection', {'label': 'Item 1'});

        // Insert time series data using Map-based columnar interface
        db.updateTimeSeriesGroup('Collection', 'data', id, {
          'date_time': ['2024-01-01T10:00:00', '2024-01-01T11:00:00', '2024-01-01T12:00:00'],
          'value': [1.5, 2.5, 3.5],
        });

        final result = db.readTimeSeriesGroup('Collection', 'data', id);
        expect(result.length, equals(2)); // 2 columns
        expect(result['date_time']!.length, equals(3));
        expect(result['value']!.length, equals(3));

        // Dimension column returns DateTime objects
        expect(result['date_time']![0], equals(DateTime(2024, 1, 1, 10, 0, 0)));
        expect(result['date_time']![1], equals(DateTime(2024, 1, 1, 11, 0, 0)));
        expect(result['date_time']![2], equals(DateTime(2024, 1, 1, 12, 0, 0)));

        // Value column returns doubles
        expect(result['value']![0], equals(1.5));
        expect(result['value']![1], equals(2.5));
        expect(result['value']![2], equals(3.5));
      } finally {
        db.close();
      }
    });

    test('readTimeSeriesGroup returns empty for no data', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Collection', {'label': 'Item 1'});

        final result = db.readTimeSeriesGroup('Collection', 'data', id);
        expect(result, isEmpty);
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

        // Insert initial data using Map-based columnar interface
        db.updateTimeSeriesGroup('Collection', 'data', id, {
          'date_time': ['2024-01-01T10:00:00'],
          'value': [1.0],
        });

        var result = db.readTimeSeriesGroup('Collection', 'data', id);
        expect(result['date_time']!.length, equals(1));

        // Replace with new data
        db.updateTimeSeriesGroup('Collection', 'data', id, {
          'date_time': ['2024-02-01T10:00:00', '2024-02-01T11:00:00'],
          'value': [10.0, 20.0],
        });

        result = db.readTimeSeriesGroup('Collection', 'data', id);
        expect(result['date_time']!.length, equals(2));
        expect(result['date_time']![0], equals(DateTime(2024, 2, 1, 10, 0, 0)));
        expect(result['value']![0], equals(10.0));
      } finally {
        db.close();
      }
    });

    test('updateTimeSeriesGroup clears data with empty map', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Collection', {'label': 'Item 1'});

        // Insert data
        db.updateTimeSeriesGroup('Collection', 'data', id, {
          'date_time': ['2024-01-01T10:00:00'],
          'value': [1.0],
        });

        // Clear with empty Map
        db.updateTimeSeriesGroup('Collection', 'data', id, {});

        final result = db.readTimeSeriesGroup('Collection', 'data', id);
        expect(result, isEmpty);
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

        // Insert out of order using Map-based columnar interface
        db.updateTimeSeriesGroup('Collection', 'data', id, {
          'date_time': ['2024-01-03T10:00:00', '2024-01-01T10:00:00', '2024-01-02T10:00:00'],
          'value': [3.0, 1.0, 2.0],
        });

        final result = db.readTimeSeriesGroup('Collection', 'data', id);
        expect(result['date_time']!.length, equals(3));
        // Should be ordered by date_time (dimension column)
        expect(result['date_time']![0], equals(DateTime(2024, 1, 1, 10, 0, 0)));
        expect(result['date_time']![1], equals(DateTime(2024, 1, 2, 10, 0, 0)));
        expect(result['date_time']![2], equals(DateTime(2024, 1, 3, 10, 0, 0)));
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
          throwsA(isA<DatabaseException>()),
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
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Multi-Column Time Series', () {
    test('multi-column update and read with mixed types', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'mixed_time_series.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Sensor', {'label': 'Sensor 1'});

        db.updateTimeSeriesGroup('Sensor', 'readings', id, {
          'date_time': ['2024-01-01T10:00:00', '2024-01-01T11:00:00'],
          'temperature': [20.5, 21.3],
          'humidity': [45, 50],
          'status': ['normal', 'high'],
        });

        final result = db.readTimeSeriesGroup('Sensor', 'readings', id);
        expect(result.length, equals(4)); // 4 columns

        // Dimension column: DateTime
        expect(result['date_time']![0], equals(DateTime(2024, 1, 1, 10, 0, 0)));
        expect(result['date_time']![1], equals(DateTime(2024, 1, 1, 11, 0, 0)));
        expect(result['date_time'], isA<List<DateTime>>());

        // REAL column: double
        expect(result['temperature']![0], equals(20.5));
        expect(result['temperature']![1], equals(21.3));
        expect(result['temperature'], isA<List<double>>());

        // INTEGER column: int
        expect(result['humidity']![0], equals(45));
        expect(result['humidity']![1], equals(50));
        expect(result['humidity'], isA<List<int>>());

        // TEXT column: String
        expect(result['status']![0], equals('normal'));
        expect(result['status']![1], equals('high'));
        expect(result['status'], isA<List<String>>());
      } finally {
        db.close();
      }
    });

    test('multi-column read returns empty map for no data', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'mixed_time_series.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Sensor', {'label': 'Sensor 1'});

        final result = db.readTimeSeriesGroup('Sensor', 'readings', id);
        expect(result, isEmpty);
      } finally {
        db.close();
      }
    });

    test('multi-column update replaces all rows', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'mixed_time_series.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Sensor', {'label': 'Sensor 1'});

        // First update
        db.updateTimeSeriesGroup('Sensor', 'readings', id, {
          'date_time': ['2024-01-01T10:00:00'],
          'temperature': [20.0],
          'humidity': [45],
          'status': ['normal'],
        });

        // Second update fully replaces first
        db.updateTimeSeriesGroup('Sensor', 'readings', id, {
          'date_time': ['2024-06-01T12:00:00', '2024-06-01T13:00:00'],
          'temperature': [30.0, 31.5],
          'humidity': [35, 40],
          'status': ['hot', 'very_hot'],
        });

        final result = db.readTimeSeriesGroup('Sensor', 'readings', id);
        expect(result['date_time']!.length, equals(2));
        expect(result['temperature'], equals([30.0, 31.5]));
        expect(result['humidity'], equals([35, 40]));
        expect(result['status'], equals(['hot', 'very_hot']));
      } finally {
        db.close();
      }
    });

    test('multi-column clear with empty map', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'mixed_time_series.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Sensor', {'label': 'Sensor 1'});

        // Insert data
        db.updateTimeSeriesGroup('Sensor', 'readings', id, {
          'date_time': ['2024-01-01T10:00:00'],
          'temperature': [20.0],
          'humidity': [45],
          'status': ['normal'],
        });

        // Clear with empty Map
        db.updateTimeSeriesGroup('Sensor', 'readings', id, {});

        final result = db.readTimeSeriesGroup('Sensor', 'readings', id);
        expect(result, isEmpty);
      } finally {
        db.close();
      }
    });

    test('multi-column ordering by dimension column', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'mixed_time_series.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Sensor', {'label': 'Sensor 1'});

        // Insert out of order
        db.updateTimeSeriesGroup('Sensor', 'readings', id, {
          'date_time': ['2024-01-03T10:00:00', '2024-01-01T10:00:00', '2024-01-02T10:00:00'],
          'temperature': [22.0, 20.0, 21.0],
          'humidity': [55, 45, 50],
          'status': ['high', 'normal', 'medium'],
        });

        final result = db.readTimeSeriesGroup('Sensor', 'readings', id);
        expect(result['date_time']!.length, equals(3));
        // Should be ordered by date_time (dimension column)
        expect(result['date_time']![0], equals(DateTime(2024, 1, 1, 10, 0, 0)));
        expect(result['date_time']![1], equals(DateTime(2024, 1, 2, 10, 0, 0)));
        expect(result['date_time']![2], equals(DateTime(2024, 1, 3, 10, 0, 0)));
        // Value columns should follow ordering
        expect(result['temperature'], equals([20.0, 21.0, 22.0]));
        expect(result['humidity'], equals([45, 50, 55]));
        expect(result['status'], equals(['normal', 'medium', 'high']));
      } finally {
        db.close();
      }
    });

    test('DateTime dimension column round-trip', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'mixed_time_series.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Sensor', {'label': 'Sensor 1'});

        // Update with string dates
        db.updateTimeSeriesGroup('Sensor', 'readings', id, {
          'date_time': ['2024-03-20T08:15:00'],
          'temperature': [18.0],
          'humidity': [70],
          'status': ['damp'],
        });

        // Read back as DateTime objects
        final result = db.readTimeSeriesGroup('Sensor', 'readings', id);
        expect(result['date_time']![0], equals(DateTime(2024, 3, 20, 8, 15, 0)));
        expect(result['date_time'], isA<List<DateTime>>());
      } finally {
        db.close();
      }
    });

    test('update with DateTime objects in dimension column', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'mixed_time_series.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Sensor', {'label': 'Sensor 1'});

        final dt = DateTime(2024, 6, 15, 14, 30, 0);

        // Update with DateTime objects in dimension column
        db.updateTimeSeriesGroup('Sensor', 'readings', id, {
          'date_time': [dt],
          'temperature': [25.0],
          'humidity': [60],
          'status': ['ok'],
        });

        // Read back and verify round-trip
        final result = db.readTimeSeriesGroup('Sensor', 'readings', id);
        expect(result['date_time']![0], equals(dt));
        expect(result['date_time'], isA<List<DateTime>>());
      } finally {
        db.close();
      }
    });

    test('rejects mismatched list lengths', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'mixed_time_series.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Sensor', {'label': 'Sensor 1'});

        expect(
          () => db.updateTimeSeriesGroup('Sensor', 'readings', id, {
            'date_time': ['2024-01-01T10:00:00', '2024-01-01T11:00:00'],
            'temperature': [20.5], // length 1 vs length 2
            'humidity': [45, 50],
            'status': ['normal', 'high'],
          }),
          throwsA(isA<ArgumentError>()),
        );
      } finally {
        db.close();
      }
    });
  });
}
