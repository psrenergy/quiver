/// DateTime conversion utilities for Quiver.
///
/// Provides conversion between Dart DateTime and ISO 8601 format strings
/// (YYYY-MM-DDTHH:MM:SS) used by Quiver database.

/// Converts a DateTime to ISO 8601 format string (YYYY-MM-DDTHH:MM:SS).
String dateTimeToString(DateTime dt) =>
    '${dt.year.toString().padLeft(4, '0')}-'
    '${dt.month.toString().padLeft(2, '0')}-'
    '${dt.day.toString().padLeft(2, '0')}T'
    '${dt.hour.toString().padLeft(2, '0')}:'
    '${dt.minute.toString().padLeft(2, '0')}:'
    '${dt.second.toString().padLeft(2, '0')}';

/// The core's DATE_TIME grammar (`datetime::is_valid_iso8601`, `src/utils/datetime.h`):
/// `YYYY-MM-DD` optionally followed by `THH:MM:SS` or ` HH:MM:SS`, every field fixed-width and
/// zero-padded, year 0001-9999. `DateTime.parse` is wider -- it accepts `YYYYMMDD`, a `Z` suffix
/// and a UTC offset, none of which Julia's parser reads. Rejecting the offset forms also keeps
/// every returned DateTime `isUtc == false`: Dart's `==` compares `isUtc` as well as the instant,
/// so a list mixing the two has same-moment values comparing unequal and deduping to two in a Set.
final _dateTimePattern = RegExp(
  r'^(?!0000)\d{4}-\d{2}-\d{2}([T ]\d{2}:\d{2}:\d{2})?$',
);

/// Converts an ISO 8601 format string (YYYY-MM-DDTHH:MM:SS) to DateTime.
///
/// Throws [ArgumentError] on a value outside the core's grammar, naming
/// `collection.attribute` when one is given.
DateTime stringToDateTime(
  String s, [
  String collection = '',
  String attribute = '',
]) {
  final parsed = _dateTimePattern.hasMatch(s) ? DateTime.parse(s) : null;
  // `DateTime.parse` rolls an out-of-range field over instead of rejecting it ('2024-02-31' reads
  // as March 2, hour 25 as the next day) where Julia and Python both throw, so re-serialize and
  // compare: only a value the parse left untouched is in the grammar.
  final normalized = s.length == 10 ? '${s}T00:00:00' : s.replaceFirst(' ', 'T');
  if (parsed == null || dateTimeToString(parsed) != normalized) {
    final source = collection.isEmpty ? '' : " in '$collection.$attribute'";
    throw ArgumentError(
      'Cannot convert "$s" to a date time$source: expected a valid YYYY-MM-DD[THH:MM:SS]',
    );
  }
  return parsed;
}
