// The rules are core's; the scopes are Jot's own.
import { config } from "./core/tooling/commitlint.mjs";

export default config([
  "core", // libjotcore: vault, index, search, sync
  "gui", // jot-gui C++ entry point
  "qml", // QML views and components
  "editor", // the Markdown editor and its highlighter
  "theme", // the design system
  "cli", // the jot command-line interface
  "sync", // sync engine and its transports
  "import", // Joplin and other importers
  "packaging", // rpm spec, debian/, repo metadata
  "data", // desktop entry, AppStream, icons
  "site", // jotwriter.org
  "ci",
  "deps",
  "deps-dev", // Dependabot's updates to development dependencies
]);
