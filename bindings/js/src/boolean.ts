export function integerToBoolean(value: number): boolean;
export function integerToBoolean(value: null): null;
export function integerToBoolean(value: number | null): boolean | null;
export function integerToBoolean(value: number | null): boolean | null {
  if (value === null) return null;
  if (value === 0) return false;
  if (value === 1) return true;
  throw new RangeError(`Cannot convert integer ${value} to boolean: expected 0 or 1`);
}
