import 'package:quiverdb/quiverdb.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

void main() {
  // Path to central tests folder
  final testsPath = path.join(path.current, '..', '..', 'tests');

  group('Run model', () {
    // The happy path (a real exit code from CarbSteeler) is not testable hermetically because
    // the model directory is a hardcoded constant in the native layer; it is verified manually.
    // The in-memory precondition fires before the model directory is consulted, so it is
    // deterministic on every machine/CI runner.
    test('throws on in-memory database', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        expect(() => db.runModel(), throwsA(isA<DatabaseException>()));
      } finally {
        db.close();
      }
    });
  });
}
