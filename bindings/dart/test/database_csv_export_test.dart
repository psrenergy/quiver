import 'dart:io';

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

  final schemaPath = path.join(testsPath, 'schemas', 'valid', 'csv_export.sql');

  group('CSV Export', () {
    test('scalar export with defaults', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      final csvPath = '${Directory.systemTemp.path}/quiver_dart_csv_scalar.csv';
      try {
        db.createElement('Items', {
          'label': 'Item1',
          'name': 'Alpha',
          'status': 1,
          'price': 9.99,
          'date_created': '2024-01-15T10:30:00',
          'notes': 'first',
        });
        db.createElement('Items', {
          'label': 'Item2',
          'name': 'Beta',
          'status': 2,
          'price': 19.5,
          'date_created': '2024-02-20T08:00:00',
          'notes': 'second',
        });

        db.exportCSV('Items', '', csvPath);
        final content = File(csvPath).readAsStringSync();

        expect(content, contains('label,name,status,price,date_created,notes\n'));
        expect(content, contains('Item1,Alpha,1,9.99,2024-01-15T10:30:00,first\n'));
        expect(content, contains('Item2,Beta,2,19.5,2024-02-20T08:00:00,second\n'));
      } finally {
        final f = File(csvPath);
        if (f.existsSync()) f.deleteSync();
        db.close();
      }
    });

    test('group export (vector)', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      final csvPath = '${Directory.systemTemp.path}/quiver_dart_csv_vector.csv';
      try {
        final id1 = db.createElement('Items', {
          'label': 'Item1',
          'name': 'Alpha',
        });
        final id2 = db.createElement('Items', {
          'label': 'Item2',
          'name': 'Beta',
        });
        db.updateVectorFloats('Items', 'measurement', id1, [1.1, 2.2, 3.3]);
        db.updateVectorFloats('Items', 'measurement', id2, [4.4, 5.5]);

        db.exportCSV('Items', 'measurements', csvPath);
        final content = File(csvPath).readAsStringSync();

        expect(content, contains('sep=,\nid,vector_index,measurement\n'));
        expect(content, contains('Item1,1,1.1\n'));
        expect(content, contains('Item1,2,2.2\n'));
        expect(content, contains('Item1,3,3.3\n'));
        expect(content, contains('Item2,1,4.4\n'));
        expect(content, contains('Item2,2,5.5\n'));
      } finally {
        final f = File(csvPath);
        if (f.existsSync()) f.deleteSync();
        db.close();
      }
    });

    test('enum labels', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      final csvPath = '${Directory.systemTemp.path}/quiver_dart_csv_enum.csv';
      try {
        db.createElement('Items', {
          'label': 'Item1',
          'name': 'Alpha',
          'status': 1,
        });
        db.createElement('Items', {
          'label': 'Item2',
          'name': 'Beta',
          'status': 2,
        });

        db.exportCSV(
          'Items',
          '',
          csvPath,
          enumLabels: {
            'status': {
              'en': {'Active': 1, 'Inactive': 2},
            },
          },
        );
        final content = File(csvPath).readAsStringSync();

        expect(content, contains('Item1,Alpha,Active,'));
        expect(content, contains('Item2,Beta,Inactive,'));
        // Raw integers should NOT be present as status values
        expect(content, isNot(contains('Item1,Alpha,1,')));
        expect(content, isNot(contains('Item2,Beta,2,')));
      } finally {
        final f = File(csvPath);
        if (f.existsSync()) f.deleteSync();
        db.close();
      }
    });

    test('date formatting', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      final csvPath = '${Directory.systemTemp.path}/quiver_dart_csv_date.csv';
      try {
        db.createElement('Items', {
          'label': 'Item1',
          'name': 'Alpha',
          'status': 1,
          'date_created': '2024-01-15T10:30:00',
        });

        db.exportCSV(
          'Items',
          '',
          csvPath,
          dateTimeFormat: '%Y/%m/%d',
        );
        final content = File(csvPath).readAsStringSync();

        expect(content, contains('2024/01/15'));
        // Raw ISO format should NOT appear
        expect(content, isNot(contains('2024-01-15T10:30:00')));
      } finally {
        final f = File(csvPath);
        if (f.existsSync()) f.deleteSync();
        db.close();
      }
    });

    test('invalid datetime returns raw value', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      final csvPath = '${Directory.systemTemp.path}/quiver_dart_csv_invalid_date.csv';
      try {
        db.createElement('Items', {
          'label': 'Item1',
          'name': 'Alpha',
          'date_created': 'not-a-date',
        });

        db.exportCSV(
          'Items',
          '',
          csvPath,
          dateTimeFormat: '%Y/%m/%d',
        );
        final content = File(csvPath).readAsStringSync();

        // Invalid datetime should be returned as-is
        expect(content, contains('not-a-date'));
      } finally {
        final f = File(csvPath);
        if (f.existsSync()) f.deleteSync();
        db.close();
      }
    });

    test('combined options (enum_labels + date_time_format)', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      final csvPath = '${Directory.systemTemp.path}/quiver_dart_csv_combined.csv';
      try {
        db.createElement('Items', {
          'label': 'Item1',
          'name': 'Alpha',
          'status': 1,
          'date_created': '2024-01-15T10:30:00',
        });
        db.createElement('Items', {
          'label': 'Item2',
          'name': 'Beta',
          'status': 2,
          'date_created': '2024-02-20T08:00:00',
        });

        db.exportCSV(
          'Items',
          '',
          csvPath,
          enumLabels: {
            'status': {
              'en': {'Active': 1, 'Inactive': 2},
            },
          },
          dateTimeFormat: '%Y/%m/%d',
        );
        final content = File(csvPath).readAsStringSync();

        // Enum labels applied
        expect(content, contains('Item1,Alpha,Active,'));
        expect(content, contains('Item2,Beta,Inactive,'));
        // Date formatting applied
        expect(content, contains('2024/01/15'));
        expect(content, contains('2024/02/20'));
        // Raw values should NOT appear
        expect(content, isNot(contains('Item1,Alpha,1,')));
        expect(content, isNot(contains('2024-01-15T10:30:00')));
      } finally {
        final f = File(csvPath);
        if (f.existsSync()) f.deleteSync();
        db.close();
      }
    });
  });
}
