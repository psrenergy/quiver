import 'package:quiverdb/quiverdb.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

// Verify report forwarding; detailed formatting is covered by the C++ core tests.
void main() {
  final testsPath = path.join(path.current, '..', '..', 'tests');
  final schemaPath = path.join(testsPath, 'schemas', 'valid', 'collections.sql');

  Database openDb() => Database.fromSchema(':memory:', schemaPath);

  group('describe', () {
    test('returns a string', () {
      final db = openDb();
      try {
        expect(
          db.describe(),
          'Database: :memory:\nVersion: 0\n\n'
          '${db.describeCollection('Collection')}\n'
          '${db.describeCollection('Configuration')}',
        );
      } finally {
        db.close();
      }
    });
  });

  group('describeCollection', () {
    test('returns structure and statistics', () {
      final db = openDb();
      try {
        db.createElement('Collection', {'label': 'a', 'some_integer': 1});
        final report = db.describeCollection('Collection');
        expect(report, contains('some_integer (INTEGER); 1 non-null, 0 null; values {1: 1}\n'));
        expect(report, contains('some_float (REAL); 0 non-null, 1 null\n'));
        expect(report, contains('data: [date_time], value(REAL); 0/1 non-empty\n'));
      } finally {
        db.close();
      }
    });
  });
}
