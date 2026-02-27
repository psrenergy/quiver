import 'package:quiverdb/quiverdb.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

void main() {
  final testsPath = path.join(path.current, '..', '..', 'tests');
  final issuesPath = path.join(testsPath, 'schemas', 'issues');

  group('Issue Regressions', () {
    test('issue 52', () {
      expect(
        () => Database.fromMigrations(
          ':memory:',
          path.join(issuesPath, 'issue52'),
        ),
        throwsA(
          isA<DatabaseException>().having(
            (e) => e.message,
            'message',
            contains('label'),
          ),
        ),
      );
    });

    test('issue 70', () {
      final db = Database.fromMigrations(
        ':memory:',
        path.join(issuesPath, 'issue70'),
      );
      try {
        db.createElement('Collection', {
          'label': 'label',
          'some_time_series': {
            'date_time': [DateTime(1990, 1, 1)],
            'some_time_series_float': [1.0],
            'some_time_series_integer': [1],
          },
        });
      } finally {
        db.close();
      }
    });
  });
}
