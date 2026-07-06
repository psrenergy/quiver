import 'package:quiverdb/quiverdb.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

// The binding only verifies each method returns a string; the report content is
// covered by the C++ core tests (tests/test_database_describe.cpp).
void main() {
  final testsPath = path.join(path.current, '..', '..', 'tests');
  final schemaPath = path.join(testsPath, 'schemas', 'valid', 'collections.sql');

  Database openDb() => Database.fromSchema(':memory:', schemaPath);

  group('describe', () {
    test('returns a string', () {
      final db = openDb();
      try {
        expect(db.describe(), isA<String>());
      } finally {
        db.close();
      }
    });
  });

  group('describeCollection', () {
    test('returns a string', () {
      final db = openDb();
      try {
        expect(db.describeCollection('Collection'), isA<String>());
      } finally {
        db.close();
      }
    });
  });

  group('summarizeCollection', () {
    test('returns a string', () {
      final db = openDb();
      try {
        expect(db.summarizeCollection('Collection'), isA<String>());
      } finally {
        db.close();
      }
    });
  });
}
