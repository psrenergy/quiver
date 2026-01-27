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

/// Converts an ISO 8601 format string (YYYY-MM-DDTHH:MM:SS) to DateTime.
DateTime stringToDateTime(String s) => DateTime.parse(s);
