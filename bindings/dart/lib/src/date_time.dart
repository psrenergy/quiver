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
  r'^(\d{4})-(\d{2})-(\d{2})(?:[T ](\d{2}):(\d{2}):(\d{2}))?$',
);

/// Converts an ISO 8601 format string (YYYY-MM-DDTHH:MM:SS) to a local DateTime.
///
/// Throws [ArgumentError] on a value outside the core's grammar, naming
/// `collection.attribute` when one is given.
DateTime stringToDateTime(
  String s, [
  String collection = '',
  String attribute = '',
]) {
  final match = _dateTimePattern.firstMatch(s);
  if (match != null) {
    final year = int.parse(match[1]!);
    final month = int.parse(match[2]!);
    final day = int.parse(match[3]!);
    final hour = match[4] == null ? 0 : int.parse(match[4]!);
    final minute = match[5] == null ? 0 : int.parse(match[5]!);
    final second = match[6] == null ? 0 : int.parse(match[6]!);
    // Dart rolls an out-of-range field over instead of rejecting it ('2024-02-31' reads as March
    // 2, hour 25 as the next day) where Julia and Python both throw, so the fields are checked by
    // constructing the value and comparing what came back: only a field left untouched is in
    // range. The check runs in UTC on purpose. A *local* construction (or `DateTime.parse`, which
    // is one) also moves a wall-clock time the platform's time zone considers nonexistent -- a DST
    // gap, and on Windows the historical Brazilian rules put one at midnight of 2019-01-01 -- so a
    // perfectly valid stored value would fail the comparison and be reported as malformed. UTC has
    // no gaps, so the comparison there judges the fields alone.
    final utc = DateTime.utc(year, month, day, hour, minute, second);
    if (year >= 1 &&
        utc.year == year &&
        utc.month == month &&
        utc.day == day &&
        utc.hour == hour &&
        utc.minute == minute &&
        utc.second == second) {
      return DateTime(year, month, day, hour, minute, second);
    }
  }
  final source = collection.isEmpty ? '' : " in '$collection.$attribute'";
  throw ArgumentError(
    'Cannot convert "$s" to a date time$source: expected a valid YYYY-MM-DD[THH:MM:SS]',
  );
}
