import 'package:path/path.dart' as path;
import 'package:quiverdb/quiverdb.dart';
import 'package:test/test.dart';

void main() {
  final schemaPath = path.join(
    path.current,
    '..',
    '..',
    'tests',
    'schemas',
    'valid',
    'all_types.sql',
  );

  group('Boolean convenience methods', () {
    late Database db;

    setUp(() {
      db = Database.fromSchema(':memory:', schemaPath);
    });

    tearDown(() {
      db.close();
    });

    test('reads scalar, vector, set, and query booleans', () {
      expect(db.readScalarBooleans('AllTypes', 'some_integer'), isEmpty);
      expect(db.readVectorBooleans('AllTypes', 'count_value'), isEmpty);
      expect(db.readSetBooleans('AllTypes', 'code'), isEmpty);

      final idFalse = db.createElement('AllTypes', {
        'label': 'False',
        'some_integer': 0,
        'count_value': [0, 1],
        'code': [0, 1],
      });
      final idTrue = db.createElement('AllTypes', {
        'label': 'True',
        'some_integer': 1,
        'count_value': [1, 0],
        'code': [1],
      });
      final idNull = db.createElement('AllTypes', {'label': 'Null'});

      expect(
        db.readScalarBooleans('AllTypes', 'some_integer'),
        equals([false, true, null]),
      );
      expect(
        db.readScalarBooleanById('AllTypes', 'some_integer', idFalse),
        isFalse,
      );
      expect(
        db.readScalarBooleanById('AllTypes', 'some_integer', idTrue),
        isTrue,
      );
      expect(
        db.readScalarBooleanById('AllTypes', 'some_integer', idNull),
        isNull,
      );

      expect(
        db.readVectorBooleans('AllTypes', 'count_value'),
        equals([
          [false, true],
          [true, false],
        ]),
      );
      expect(
        db.readVectorBooleansById('AllTypes', 'count_value', idFalse),
        equals([false, true]),
      );
      expect(
        db.readVectorBooleansById('AllTypes', 'count_value', idNull),
        isEmpty,
      );

      expect(
        db.readSetBooleans('AllTypes', 'code'),
        equals([
          [false, true],
          [true],
        ]),
      );
      expect(
        db.readSetBooleansById('AllTypes', 'code', idFalse),
        equals([false, true]),
      );
      expect(
        db.readSetBooleansById('AllTypes', 'code', idNull),
        isEmpty,
      );

      expect(db.queryBoolean('SELECT 0'), isFalse);
      expect(
        db.queryBoolean(
          'SELECT some_integer FROM AllTypes WHERE id = ?',
          [idTrue],
        ),
        isTrue,
      );
      expect(
        db.queryBoolean('SELECT some_integer FROM AllTypes WHERE id = -1'),
        isNull,
      );
    });

    test('rejects non-binary integers', () {
      db.createElement('AllTypes', {'label': 'Invalid', 'some_integer': 2});

      expect(
        () => db.readScalarBooleans('AllTypes', 'some_integer'),
        throwsArgumentError,
      );
      expect(() => db.queryBoolean('SELECT 2'), throwsArgumentError);
    });
  });
}
