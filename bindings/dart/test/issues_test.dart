import 'package:quiver_db/quiver_db.dart';
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
  });
}
