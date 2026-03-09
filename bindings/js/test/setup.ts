import { join, resolve } from "node:path";

const buildBinDir = resolve(join(import.meta.dir, "..", "..", "..", "build", "bin"));
const currentPath = process.env.PATH ?? "";

if (!currentPath.includes(buildBinDir)) {
  process.env.PATH = `${buildBinDir};${currentPath}`;
}
