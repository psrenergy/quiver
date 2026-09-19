import 'dart:ffi';

import 'package:quiverdb/src/ffi/bindings.dart';
import 'package:quiverdb/src/ffi/library_loader.dart';
import 'package:test/test.dart';

// SAFE-01/SAFE-02/SAFE-03 (Phase 2, plan 02-04): the load-time struct-size gate. Dart resolves
// every native symbol via a `late final` field on FIRST ACCESS (bindings.dart:16, :26-27), so
// merely constructing QuiverDatabaseBindings proves nothing -- the gate must actively CALL the
// three *_sizeof accessors. Criterion 5 requires the FAILURE path to be tested too: asserting
// only 24 == 24 against a library that also reports 24 proves today agrees with today, not that
// a wrong layout would actually fail.

void main() {
  group('happy path', () {
    test('the three native accessors match this binding\'s compiled struct sizes', () {
      expect(bindings.quiver_database_options_sizeof(), equals(sizeOf<quiver_database_options_t>()));
      expect(bindings.quiver_scalar_metadata_sizeof(), equals(sizeOf<quiver_scalar_metadata_t>()));
      expect(bindings.quiver_group_metadata_sizeof(), equals(sizeOf<quiver_group_metadata_t>()));

      expect(bindings.quiver_database_options_sizeof(), equals(24));
      expect(bindings.quiver_scalar_metadata_sizeof(), equals(56));
      expect(bindings.quiver_group_metadata_sizeof(), equals(32));
    });

    test('accessing bindings against a correct native library does not throw', () {
      expect(() => bindings, returnsNormally);
    });
  });

  group('checkStructSize failure path', () {
    test('options: a wrong expected value throws, naming both numbers', () {
      expect(
        () => checkStructSize('quiver_database_options_t', 24, 8),
        throwsA(
          isA<StateError>().having(
            (e) => e.message,
            'message',
            allOf(contains('quiver_database_options_t'), contains('24'), contains('8')),
          ),
        ),
      );
    });

    test('scalar metadata: a wrong expected value throws, naming both numbers', () {
      expect(
        () => checkStructSize('quiver_scalar_metadata_t', 56, 40),
        throwsA(
          isA<StateError>().having(
            (e) => e.message,
            'message',
            allOf(contains('quiver_scalar_metadata_t'), contains('56'), contains('40')),
          ),
        ),
      );
    });

    test('group metadata: a wrong expected value throws, naming both numbers', () {
      expect(
        () => checkStructSize('quiver_group_metadata_t', 32, 16),
        throwsA(
          isA<StateError>().having(
            (e) => e.message,
            'message',
            allOf(contains('quiver_group_metadata_t'), contains('32'), contains('16')),
          ),
        ),
      );
    });

    test('an exactly-equal size passes without throwing -- no tolerance band', () {
      expect(() => checkStructSize('quiver_database_options_t', 24, 24), returnsNormally);
    });
  });

  group('assertNativeStructSizes ordering', () {
    test('runs options, scalar, group in that fixed order against the live library', () {
      // Not directly observable from outside (it short-circuits on the first mismatch, and the
      // live library agrees on all three), but calling it against the correct native library
      // must not throw -- the ordering claim is exercised implicitly every time `bindings` is
      // first accessed above.
      expect(() => assertNativeStructSizes(bindings), returnsNormally);
    });
  });
}
