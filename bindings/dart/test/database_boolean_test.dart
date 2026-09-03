import 'package:path/path.dart' as path;
import 'package:quiverdb/quiverdb.dart';
import 'package:test/test.dart';

void main() {
  final testsPath = path.join(path.current, '..', '..', 'tests');
  final schemaPath = path.join(testsPath, 'schemas', 'valid', 'all_types.sql');

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

    test('writes booleans as integers', () {
      final id = db.createElement('AllTypes', {
        'label': 'Written',
        'some_integer': true,
        'count_value': [true, false],
        'code': [true],
      });

      expect(db.readScalarBooleanById('AllTypes', 'some_integer', id), isTrue);
      expect(db.readVectorBooleansById('AllTypes', 'count_value', id), equals([true, false]));
      expect(db.readSetBooleansById('AllTypes', 'code', id), equals([true]));

      db.updateElement('AllTypes', id, {'some_integer': false});
      expect(db.readScalarBooleanById('AllTypes', 'some_integer', id), isFalse);

      expect(db.queryBoolean('SELECT some_integer FROM AllTypes WHERE some_integer = ?', [false]), isFalse);
    });

    test('writes booleans as integers through the group writers', () {
      final id = db.createElement('AllTypes', {'label': 'Grouped'});

      // updateVectorGroup, updateSetGroup, updateTimeSeriesGroup, upsertTimeSeriesRow and every
      // ByLabel form share one marshaller, so this covers the whole family.
      db.updateVectorGroup('AllTypes', 'counts', id, {
        'count_value': [true, false, true],
      });
      db.updateSetGroup('AllTypes', 'codes', id, {
        'code': [true, false],
      });

      expect(db.readVectorBooleansById('AllTypes', 'count_value', id), equals([true, false, true]));
      // A set has no insertion order.
      expect(db.readSetBooleansById('AllTypes', 'code', id), unorderedEquals([true, false]));
    });

    test('group writer rejection names the offending column', () {
      final id = db.createElement('AllTypes', {'label': 'Bad'});

      expect(
        () => db.updateVectorGroup('AllTypes', 'counts', id, {
          'count_value': [
            <int>[1],
          ],
        }),
        throwsA(
          isA<ArgumentError>().having(
            (e) => e.message,
            'message',
            allOf(contains('count_value'), contains('Unsupported value type')),
          ),
        ),
      );
    });

    test('rejects non-binary integers', () {
      final id = db.createElement('AllTypes', {
        'label': 'Invalid',
        'some_integer': 2,
        'count_value': [0, 2],
        'code': [2],
      });

      expect(
        () => db.readScalarBooleans('AllTypes', 'some_integer'),
        throwsA(isArgumentError.having((e) => e.toString(), 'message', contains('AllTypes.some_integer'))),
      );
      expect(() => db.readScalarBooleanById('AllTypes', 'some_integer', id), throwsArgumentError);
      expect(() => db.readVectorBooleans('AllTypes', 'count_value'), throwsArgumentError);
      expect(() => db.readVectorBooleansById('AllTypes', 'count_value', id), throwsArgumentError);
      expect(() => db.readSetBooleans('AllTypes', 'code'), throwsArgumentError);
      expect(() => db.readSetBooleansById('AllTypes', 'code', id), throwsArgumentError);
      expect(() => db.queryBoolean('SELECT 2'), throwsArgumentError);
    });
  });
}
