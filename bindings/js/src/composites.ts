import { Database } from "./database.js";

declare module "./database.js" {
  interface Database {
    readScalarsById(collection: string, id: number): Record<string, number | string | null>;
    readVectorsById(collection: string, id: number): Record<string, number[] | string[]>;
    readSetsById(collection: string, id: number): Record<string, number[] | string[]>;
  }
}

Database.prototype.readScalarsById = function (
  this: Database,
  collection: string,
  id: number,
): Record<string, number | string | null> {
  const attributes = this.listScalarAttributes(collection);
  const result: Record<string, number | string | null> = {};

  for (const attr of attributes) {
    switch (attr.dataType) {
      case 0: // INTEGER
        result[attr.name] = this.readScalarIntegerById(collection, attr.name, id);
        break;
      case 1: // FLOAT
        result[attr.name] = this.readScalarFloatById(collection, attr.name, id);
        break;
      case 2: // STRING
      case 3: // DATE_TIME
        result[attr.name] = this.readScalarStringById(collection, attr.name, id);
        break;
      default:
        throw new Error(`Unknown data type ${attr.dataType} for attribute '${attr.name}'`);
    }
  }

  return result;
};

Database.prototype.readVectorsById = function (
  this: Database,
  collection: string,
  id: number,
): Record<string, number[] | string[]> {
  const groups = this.listVectorGroups(collection);
  const result: Record<string, number[] | string[]> = {};

  for (const group of groups) {
    for (const col of group.valueColumns) {
      switch (col.dataType) {
        case 0: // INTEGER
          result[col.name] = this.readVectorIntegersById(collection, col.name, id);
          break;
        case 1: // FLOAT
          result[col.name] = this.readVectorFloatsById(collection, col.name, id);
          break;
        case 2: // STRING
        case 3: // DATE_TIME
          result[col.name] = this.readVectorStringsById(collection, col.name, id);
          break;
        default:
          throw new Error(`Unknown data type ${col.dataType} for column '${col.name}'`);
      }
    }
  }

  return result;
};

Database.prototype.readSetsById = function (
  this: Database,
  collection: string,
  id: number,
): Record<string, number[] | string[]> {
  const groups = this.listSetGroups(collection);
  const result: Record<string, number[] | string[]> = {};

  for (const group of groups) {
    for (const col of group.valueColumns) {
      switch (col.dataType) {
        case 0: // INTEGER
          result[col.name] = this.readSetIntegersById(collection, col.name, id);
          break;
        case 1: // FLOAT
          result[col.name] = this.readSetFloatsById(collection, col.name, id);
          break;
        case 2: // STRING
        case 3: // DATE_TIME
          result[col.name] = this.readSetStringsById(collection, col.name, id);
          break;
        default:
          throw new Error(`Unknown data type ${col.dataType} for column '${col.name}'`);
      }
    }
  }

  return result;
};
