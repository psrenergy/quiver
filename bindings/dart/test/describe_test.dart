import 'package:quiverdb/quiverdb.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

void main() {
  final testsPath = path.join(path.current, '..', '..', 'tests');
  final schemaPath = path.join(testsPath, 'schemas', 'valid', 'collections.sql');

  Database seeded() {
    final db = Database.fromSchema(':memory:', schemaPath);
    db.createElement('Collection', {
      'label': 'a',
      'some_integer': 1,
      'value_int': [10, 20],
    });
    db.createElement('Collection', {'label': 'b', 'some_integer': 1});
    db.createElement('Collection', {'label': 'c', 'some_integer': 5});
    return db;
  }

  group('describe', () {
    test('returns a human-readable overview of every collection', () {
      final db = seeded();
      try {
        final report = db.describe();
        expect(report, contains('Collection: Configuration'));
        expect(report, contains('Collection: Collection (3 elements)'));
      } finally {
        db.close();
      }
    });
  });

  group('describeCollection', () {
    test('returns the shape of one collection', () {
      final db = seeded();
      try {
        final report = db.describeCollection('Collection');
        expect(report, contains('Collection: Collection'));
        expect(report, contains('[date_time]'));
        expect(report, isNot(contains('Configuration')));
      } finally {
        db.close();
      }
    });
  });

  group('summarizeCollection', () {
    test('reports null counts, value distributions, and empty-group counts', () {
      final db = seeded();
      try {
        final report = db.summarizeCollection('Collection');
        expect(
          report,
          contains('some_integer: 3 non-null, 0 null; values {1: 2, 5: 1}'),
        );
        expect(report, contains('values: 1/3 non-empty'));
      } finally {
        db.close();
      }
    });
  });
}
