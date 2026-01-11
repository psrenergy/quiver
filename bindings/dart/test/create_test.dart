import 'package:psr_database/psr_database.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

void main() {
  // Path to central tests folder
  final testsPath = path.join(
    path.current,
    '..',
    '..',
    'tests',
    'test_create',
  );

  group('Create Parameters', () {
    test('rejects wrong type for parameter', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'test_create_parameters.sql'),
      );
      try {
        expect(
          () => db.createElement('Configuration', {
            'label': 'Toy Case',
            'value1': 'wrong', // Should be double, not string
          }),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });

    test('creates element with valid parameters', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'test_create_parameters.sql'),
      );
      try {
        final id = db.createElement('Configuration', {
          'label': 'Toy Case',
          'value1': 1.0,
        });
        expect(id, greaterThan(0));
      } finally {
        db.close();
      }
    });

    test('creates element with default values', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'test_create_parameters.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Toy Case', 'value1': 1.0});
        final id = db.createElement('Resource', {'label': 'Resource 2'});
        expect(id, greaterThan(0));
      } finally {
        db.close();
      }
    });

    test('creates element with enum value', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'test_create_parameters.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Toy Case', 'value1': 1.0});
        final id = db.createElement('Resource', {
          'label': 'Resource 1',
          'type': 'E',
        });
        expect(id, greaterThan(0));
      } finally {
        db.close();
      }
    });

    test('rejects unknown attribute', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'test_create_parameters.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Toy Case', 'value1': 1.0});
        expect(
          () => db.createElement('Resource', {
            'label': 'Resource 4',
            'type3': 'E', // Unknown attribute
          }),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Create Empty Parameters', () {
    test('creates element with only label', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'test_create_parameters.sql'),
      );
      try {
        final id = db.createElement('Configuration', {'label': 'Toy Case'});
        expect(id, greaterThan(0));
      } finally {
        db.close();
      }
    });

    test('rejects element without label', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'test_create_parameters.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Toy Case'});
        expect(
          () => db.createElement('Resource', {}), // Missing required label
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Create Parameters and Vectors', () {
    test('creates element with vector attribute', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'test_create_parameters_and_vectors.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Toy Case', 'value1': 1.0});
        final id = db.createElement('Resource', {
          'label': 'Resource 1',
          'type': 'E',
          'some_value': [1.0, 2.0, 3.0],
        });
        expect(id, greaterThan(0));
      } finally {
        db.close();
      }
    });

    test('creates multiple elements with vectors', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'test_create_parameters_and_vectors.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Toy Case', 'value1': 1.0});
        db.createElement('Resource', {
          'label': 'Resource 1',
          'type': 'E',
          'some_value': [1.0, 2.0, 3.0],
        });
        db.createElement('Cost', {'label': 'Cost 1', 'value': 30.0});
        db.createElement('Cost', {'label': 'Cost 2', 'value': 20.0});

        final id1 = db.createElement('Plant', {
          'label': 'Plant 1',
          'capacity': 50.0,
          'some_factor': [0.1, 0.3],
        });
        expect(id1, greaterThan(0));

        final id2 = db.createElement('Plant', {
          'label': 'Plant 2',
          'capacity': 50.0,
          'some_factor': [0.1, 0.3, 0.5],
        });
        expect(id2, greaterThan(0));
      } finally {
        db.close();
      }
    });

    test('rejects time series file attribute', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'test_create_parameters_and_vectors.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Toy Case', 'value1': 1.0});
        expect(
          () => db.createElement('Plant', {
            'label': 'Plant 3',
            'generation': 'some_file.txt', // Time series not supported
          }),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });

    test('rejects empty vector', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'test_create_parameters_and_vectors.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Toy Case', 'value1': 1.0});
        expect(
          () => db.createElement('Plant', {
            'label': 'Plant 4',
            'capacity': 50.0,
            'some_factor': <double>[], // Empty vector not allowed
          }),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });

    test('creates element with foreign key reference', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'test_create_parameters_and_vectors.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Toy Case', 'value1': 1.0});
        db.createElement('Resource', {
          'label': 'Resource 1',
          'type': 'E',
          'some_value': [1.0, 2.0, 3.0],
        });

        final id = db.createElement('Plant', {
          'label': 'Plant 3',
          'resource_id': 1,
        });
        expect(id, greaterThan(0));
      } finally {
        db.close();
      }
    });

    test('rejects scalar value for vector attribute', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'test_create_parameters_and_vectors.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Toy Case', 'value1': 1.0});
        expect(
          () => db.createElement('Resource', {
            'label': 'Resource 1',
            'type': 'E',
            'some_value': 1.0, // Should be a list, not scalar
          }),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Create Sets with Relations', () {
    test('creates element with set relation', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'test_create_sets_with_relations.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Toy Case',
          'some_value': 1.0,
        });
        db.createElement('Product', {'label': 'Sugar', 'unit': 'Kg'});
        db.createElement('Product', {'label': 'Sugarcane', 'unit': 'ton'});
        db.createElement('Product', {'label': 'Molasse', 'unit': 'ton'});
        db.createElement('Product', {'label': 'Bagasse', 'unit': 'ton'});

        final id = db.createElement('Process', {
          'label': 'Sugar Mill',
          'product_set': ['Sugarcane'],
          'factor': [1.0],
        });
        expect(id, greaterThan(0));
      } finally {
        db.close();
      }
    });

    test('rejects wrong type in unit attribute', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'test_create_sets_with_relations.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Toy Case',
          'some_value': 1.0,
        });
        db.createElement('Product', {'label': 'Sugar', 'unit': 'Kg'});
        expect(
          () => db.createElement('Product', {
            'label': 'Bagasse 2',
            'unit': 30, // Should be string, not int
          }),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });

    test('rejects wrong type in set factor', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'test_create_sets_with_relations.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Toy Case',
          'some_value': 1.0,
        });
        db.createElement('Product', {'label': 'Sugar', 'unit': 'Kg'});
        expect(
          () => db.createElement('Process', {
            'label': 'Sugar Mill 2',
            'product_set': ['Sugar'],
            'factor': ['wrong'], // Should be double, not string
          }),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });

    test('rejects non-existent relation reference', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'test_create_sets_with_relations.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Toy Case',
          'some_value': 1.0,
        });
        db.createElement('Product', {'label': 'Sugar', 'unit': 'Kg'});
        expect(
          () => db.createElement('Process', {
            'label': 'Sugar Mill 3',
            'product_set': ['Some Sugar'], // Non-existent product
            'factor': [1.0],
          }),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });

    test('rejects empty factor with set', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'test_create_sets_with_relations.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Toy Case',
          'some_value': 1.0,
        });
        db.createElement('Product', {'label': 'Sugar', 'unit': 'Kg'});
        expect(
          () => db.createElement('Process', {
            'label': 'Sugar Mill 3',
            'product_set': ['Sugar'],
            'factor': <double>[], // Empty factor not allowed
          }),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });
}
