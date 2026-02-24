import 'dart:io';

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

  group('CSV Import', () {
    test('scalar round-trip', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      final csvPath = '${Directory.systemTemp.path}/quiver_dart_csv_import_scalar.csv';
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

        // Import into fresh DB
        final db2 = Database.fromSchema(':memory:', schemaPath);
        try {
          db2.importCSV('Items', '', csvPath);

          final names = db2.readScalarStrings('Items', 'name');
          expect(names.length, 2);
          expect(names[0], 'Alpha');
          expect(names[1], 'Beta');
        } finally {
          db2.close();
        }
      } finally {
        final f = File(csvPath);
        if (f.existsSync()) f.deleteSync();
        db.close();
      }
    });

    test('vector group round-trip', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      final csvPath = '${Directory.systemTemp.path}/quiver_dart_csv_import_vector.csv';
      try {
        final id1 = db.createElement('Items', {
          'label': 'Item1',
          'name': 'Alpha',
        });
        db.updateVectorFloats('Items', 'measurement', id1, [1.1, 2.2, 3.3]);

        db.exportCSV('Items', 'measurements', csvPath);

        // Clear and re-import
        db.updateVectorFloats('Items', 'measurement', id1, []);
        db.importCSV('Items', 'measurements', csvPath);

        final vals = db.readVectorFloatsById('Items', 'measurement', id1);
        expect(vals.length, 3);
        expect(vals[0], closeTo(1.1, 0.001));
        expect(vals[1], closeTo(2.2, 0.001));
        expect(vals[2], closeTo(3.3, 0.001));
      } finally {
        final f = File(csvPath);
        if (f.existsSync()) f.deleteSync();
        db.close();
      }
    });

    test('scalar header-only clears table', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      final csvPath = '${Directory.systemTemp.path}/quiver_dart_csv_import_header.csv';
      try {
        db.createElement('Items', {
          'label': 'Item1',
          'name': 'Alpha',
        });

        // Write header-only CSV
        File(csvPath).writeAsStringSync('sep=,\nlabel,name,status,price,date_created,notes\n');

        db.importCSV('Items', '', csvPath);

        final names = db.readScalarStrings('Items', 'name');
        expect(names, isEmpty);
      } finally {
        final f = File(csvPath);
        if (f.existsSync()) f.deleteSync();
        db.close();
      }
    });

    test('enum resolution', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      final csvPath = '${Directory.systemTemp.path}/quiver_dart_csv_import_enum.csv';
      try {
        File(csvPath).writeAsStringSync('sep=,\nlabel,name,status,price,date_created,notes\nItem1,Alpha,Active,,,\n');

        db.importCSV(
          'Items',
          '',
          csvPath,
          enumLabels: {
            'status': {
              'en': {'Active': 1, 'Inactive': 2},
            },
          },
        );

        final status = db.readScalarIntegerById('Items', 'status', 1);
        expect(status, isNotNull);
        expect(status, 1);
      } finally {
        final f = File(csvPath);
        if (f.existsSync()) f.deleteSync();
        db.close();
      }
    });

    test('datetime format', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      final csvPath = '${Directory.systemTemp.path}/quiver_dart_csv_import_date.csv';
      try {
        File(
          csvPath,
        ).writeAsStringSync('sep=,\nlabel,name,status,price,date_created,notes\nItem1,Alpha,,,2024/01/15,\n');

        db.importCSV(
          'Items',
          '',
          csvPath,
          dateTimeFormat: '%Y/%m/%d',
        );

        final date = db.readScalarStringById('Items', 'date_created', 1);
        expect(date, isNotNull);
        expect(date, '2024-01-15T00:00:00');
      } finally {
        final f = File(csvPath);
        if (f.existsSync()) f.deleteSync();
        db.close();
      }
    });

    test('semicolon sep= header round-trip', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      final csvPath = '${Directory.systemTemp.path}/quiver_dart_csv_import_semicolon.csv';
      try {
        File(csvPath).writeAsStringSync(
          'sep=;\nlabel;name;status;price;date_created;notes\nItem1;Alpha;1;9.99;;\n',
        );

        db.importCSV('Items', '', csvPath);

        final names = db.readScalarStrings('Items', 'name');
        expect(names.length, 1);
        expect(names[0], 'Alpha');
      } finally {
        final f = File(csvPath);
        if (f.existsSync()) f.deleteSync();
        db.close();
      }
    });

    test('semicolon auto-detect round-trip', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      final csvPath = '${Directory.systemTemp.path}/quiver_dart_csv_import_semicolon_auto.csv';
      try {
        File(csvPath).writeAsStringSync(
          'label;name;status;price;date_created;notes\nItem1;Alpha;1;9.99;;\n',
        );

        db.importCSV('Items', '', csvPath);

        final names = db.readScalarStrings('Items', 'name');
        expect(names.length, 1);
        expect(names[0], 'Alpha');
      } finally {
        final f = File(csvPath);
        if (f.existsSync()) f.deleteSync();
        db.close();
      }
    });

    test('cannot open file throws', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      try {
        expect(
          () => db.importCSV('Items', '', '/nonexistent/path/file.csv'),
          throwsException,
        );
      } finally {
        db.close();
      }
    });

    test('group duplicate entries throws', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      final csvPath = '${Directory.systemTemp.path}/quiver_dart_csv_import_dup_group.csv';
      try {
        db.createElement('Items', {
          'label': 'Item1',
          'name': 'Alpha',
        });

        File(csvPath).writeAsStringSync('sep=,\nid,tag\nItem1,red\nItem1,red\n');

        expect(
          () => db.importCSV('Items', 'tags', csvPath),
          throwsException,
        );
      } finally {
        final f = File(csvPath);
        if (f.existsSync()) f.deleteSync();
        db.close();
      }
    });

    test('group invalid enum throws', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      final csvPath = '${Directory.systemTemp.path}/quiver_dart_csv_import_bad_group_enum.csv';
      try {
        db.createElement('Items', {
          'label': 'Item1',
          'name': 'Alpha',
        });

        File(csvPath).writeAsStringSync(
          'sep=,\nid,date_time,temperature,humidity\nItem1,2024-01-01T10:00:00,22.5,Unknown\n',
        );

        expect(
          () => db.importCSV(
            'Items',
            'readings',
            csvPath,
            enumLabels: {
              'humidity': {
                'en': {'Low': 60, 'High': 90},
              },
            },
          ),
          throwsException,
        );
      } finally {
        final f = File(csvPath);
        if (f.existsSync()) f.deleteSync();
        db.close();
      }
    });
  });
}
