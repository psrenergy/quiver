import 'package:quiverdb/quiverdb.dart';
import 'package:test/test.dart';

void main() {
  group('dateTimeToString', () {
    test('formats as YYYY-MM-DDTHH:MM:SS with zero-padded fields', () {
      expect(
        dateTimeToString(DateTime(2024, 1, 5, 3, 7, 9)),
        equals('2024-01-05T03:07:09'),
      );
      expect(
        dateTimeToString(DateTime(2024, 12, 31, 23, 59, 59)),
        equals('2024-12-31T23:59:59'),
      );
    });

    test('pads the year to four digits', () {
      expect(dateTimeToString(DateTime(1, 1, 1)), equals('0001-01-01T00:00:00'));
      expect(dateTimeToString(DateTime(999, 6, 15)), equals('0999-06-15T00:00:00'));
    });

    test('a date-only DateTime formats as midnight', () {
      expect(
        dateTimeToString(DateTime(2024, 1, 15)),
        equals('2024-01-15T00:00:00'),
      );
    });

    test('drops sub-second precision', () {
      expect(
        dateTimeToString(DateTime(2024, 1, 15, 10, 30, 45, 999, 999)),
        equals('2024-01-15T10:30:45'),
      );
    });

    test('formats a UTC DateTime from its own fields without converting', () {
      expect(
        dateTimeToString(DateTime.utc(2024, 1, 15, 10, 30)),
        equals('2024-01-15T10:30:00'),
      );
    });
  });

  group('stringToDateTime accepts the core grammar', () {
    test('YYYY-MM-DDTHH:MM:SS', () {
      final dt = stringToDateTime('2024-01-15T10:30:45');
      expect(dt, equals(DateTime(2024, 1, 15, 10, 30, 45)));
      expect(dt.isUtc, isFalse);
    });

    test('YYYY-MM-DD HH:MM:SS (space separator)', () {
      expect(
        stringToDateTime('2024-01-15 10:30:45'),
        equals(DateTime(2024, 1, 15, 10, 30, 45)),
      );
    });

    test('YYYY-MM-DD reads as midnight', () {
      final dt = stringToDateTime('2024-01-15');
      expect(dt, equals(DateTime(2024, 1, 15)));
      expect(dt.hour, equals(0));
      expect(dt.minute, equals(0));
      expect(dt.second, equals(0));
    });

    test('the field boundaries', () {
      expect(stringToDateTime('0001-01-01'), equals(DateTime(1, 1, 1)));
      expect(
        stringToDateTime('9999-12-31T23:59:59'),
        equals(DateTime(9999, 12, 31, 23, 59, 59)),
      );
      expect(
        stringToDateTime('2024-01-01T00:00:00'),
        equals(DateTime(2024, 1, 1, 0, 0, 0)),
      );
    });

    test('a leap day in a leap year', () {
      expect(stringToDateTime('2024-02-29'), equals(DateTime(2024, 2, 29)));
      expect(stringToDateTime('2000-02-29'), equals(DateTime(2000, 2, 29)));
    });

    test('round-trips through dateTimeToString', () {
      for (final s in [
        '2024-01-15T10:30:45',
        '1970-01-01T00:00:00',
        '2024-06-20T14:45:30',
      ]) {
        expect(dateTimeToString(stringToDateTime(s)), equals(s), reason: s);
      }
      // The date-only form canonicalizes to midnight.
      expect(
        dateTimeToString(stringToDateTime('2024-01-15')),
        equals('2024-01-15T00:00:00'),
      );
    });
  });

  group('stringToDateTime rejects values outside the core grammar', () {
    // Every value here is rejected by the core write-side gate (`datetime::is_valid_iso8601`)
    // and by Julia's `string_to_date_time` and Python's `_parse_datetime`; the Dart reader has
    // to agree so that a stored value is readable everywhere and an unreadable one is refused
    // everywhere.
    const bad = [
      // Wrong shape.
      '',
      '2024',
      '2024-01',
      '20240115',
      '2024-1-5',
      '2024-01-15T10:30',
      '2024-01-15T10:30:00.000',
      '2024-01-15t10:30:00',
      ' 2024-01-15',
      '2024-01-15 ',
      '2024-01-15xyz',
      '+2024-01-15',
      'not-a-date',
      // Forms DateTime.parse accepts but the core does not: a Z suffix or a UTC offset.
      '2024-01-15T10:30:00Z',
      '2024-01-15T10:30:00+03:00',
      '2024-01-15T10:30:00-03:00',
      // Right shape, out-of-range field. DateTime.parse rolls each of these over silently.
      '0000-01-01',
      '2024-00-15',
      '2024-13-01',
      '2024-01-00',
      '2024-01-32',
      '2024-02-31',
      '2023-02-29',
      '2024-04-31',
      '2024-01-15T24:00:00',
      '2024-01-15T25:00:00',
      '2024-01-15T10:60:00',
      '2024-01-15T10:30:60',
    ];

    for (final s in bad) {
      test('"$s"', () {
        expect(
          () => stringToDateTime(s),
          throwsA(
            isArgumentError.having(
              (e) => e.toString(),
              'message',
              allOf(
                contains('Cannot convert "$s" to a date time'),
                contains('expected a valid YYYY-MM-DD[THH:MM:SS]'),
                isNot(contains(' in ')),
              ),
            ),
          ),
        );
      });
    }

    test('names the column when a collection and attribute are given', () {
      expect(
        () => stringToDateTime('2024-02-31', 'Consumption', 'date_time'),
        throwsA(
          isArgumentError.having(
            (e) => e.toString(),
            'message',
            contains(
              'Cannot convert "2024-02-31" to a date time in '
              "'Consumption.date_time': expected a valid YYYY-MM-DD[THH:MM:SS]",
            ),
          ),
        ),
      );
    });
  });

  group('stringToDateTime does not depend on the local time zone', () {
    // Regression: the parser used to validate by re-serializing a *local* parse and comparing it
    // with the input. A wall-clock time the platform's time zone considers nonexistent -- a DST
    // gap; on Windows the historical Brazilian rules put one at midnight of 2019-01-01 -- came
    // back shifted by an hour, failed the comparison, and a valid stored value was reported as
    // `Cannot convert "2019-01-01T00:00:00" to a date time ... expected a valid
    // YYYY-MM-DD[THH:MM:SS]`. Field validation now runs in UTC, so the only thing the local zone
    // can influence is the returned instant, which must be the local DateTime with these fields
    // (what `DateTime(y, m, d, h, mi, s)` builds), never an error.
    test('midnight of 2019-01-01 reads (the reported failure)', () {
      expect(
        stringToDateTime('2019-01-01T00:00:00', 'Consumption', 'date_time'),
        equals(DateTime(2019, 1, 1, 0, 0, 0)),
      );
    });
  });
}
