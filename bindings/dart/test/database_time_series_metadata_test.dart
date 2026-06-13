import 'package:quiverdb/quiverdb.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

void main() {
  // Path to central tests folder
  final testsPath = path.join(path.current, '..', '..', 'tests');

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

    test(
      'listTimeSeriesGroups returns empty for collection without time series',
      () {
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
      },
    );
  });
}
