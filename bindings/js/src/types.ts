export type ScalarValue = number | bigint | string | null;
export type ArrayValue = number[] | bigint[] | string[];
export type Value = ScalarValue | ArrayValue;
export type ElementData = Record<string, Value | undefined>;
