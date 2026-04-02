import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const __dirname = dirname(fileURLToPath(import.meta.url));
const buildBinDir = resolve(join(__dirname, "..", "..", "..", "build", "bin"));
const currentPath = process.env.PATH ?? "";

if (!currentPath.includes(buildBinDir)) {
  process.env.PATH = `${buildBinDir};${currentPath}`;
}
