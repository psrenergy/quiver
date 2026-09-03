export function integerToBoolean(value: number, collection?: string, attribute?: string): boolean;
export function integerToBoolean(value: null, collection?: string, attribute?: string): null;
export function integerToBoolean(
  value: number | null,
  collection?: string,
  attribute?: string,
): boolean | null;
export function integerToBoolean(
  value: number | null,
  collection?: string,
  attribute?: string,
): boolean | null {
  if (value === null) return null;
  if (value === 0) return false;
  if (value === 1) return true;
  const source = collection ? ` in '${collection}.${attribute}'` : "";
  // A RangeError, not a QuiverError: the message is crafted here, not read from
  // quiver_get_last_error — these readers are a binding-only convenience.
  throw new RangeError(`Cannot convert integer ${value} to boolean${source}: expected 0 or 1`);
}
