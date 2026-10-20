# Jot

Plain-file Markdown notes for Linux. Qt6 + QML for the UI, C++ for the core. MIT licensed. App ID
`org.callieapp.Jot`, homepage `jotwriter.org`. Jot shares its design and tooling with
[Callie](https://github.com/greylocklabs/callie); [docs/SUITE.md](docs/SUITE.md) says how.

## Layout

| Path           | What                                                                                                 |
| -------------- | ---------------------------------------------------------------------------------------------------- |
| `src/core/`    | `libjotcore`: vault, index, search and sync. No UI and no QML types.                                 |
| `src/ui/`      | The QML module `Jot.Ui`: views, components, and the `Theme` singleton.                               |
| `src/gui/`     | `jot-gui` entry point. `main.cpp` only.                                                              |
| `src/cli/`     | `jot`, the command-line interface over the same core.                                                |
| `src/gallery/` | `jot-gallery`, a development tool that renders a theme's tokens and components.                      |
| `tests/`       | Qt Test cases run by ctest.                                                                          |
| `core/`        | Submodule from greylocklabs/core: the built-in themes, release and format scripts, and lint configs. |
| `data/`        | Desktop entry, AppStream metainfo and bundled fonts.                                                 |
| `assets/`      | Logo, also installed as the application icon, with hand-drawn 16 and 32 px sizes.                    |
| `packaging/`   | Hand-written `rpm/jot.spec` and `debian/`, at distro-review quality.                                 |
| `docs/`        | The roadmap, the suite rules and background research.                                                |

## Conventions

Read `CONTRIBUTING.md` before writing commits or comments. In short:

- Conventional Commits, enforced by commitlint. Lower-case subject, no trailing period,
  72 character header.
- Comments are one or two sentences and explain why, not what. No em dashes anywhere in
  comments, docs, or commit messages.
- Notes are plain files and stay the source of truth. Anything Jot stores beside them is a cache
  that can be rebuilt.
- Text and layout arithmetic belongs in C++ models, not QML.
- Qt 6.8 (Debian trixie) cannot add a bare `u"..."` literal to a `QString`; use `QStringLiteral` or
  the `_s` suffix. Newer Qt accepts it, so only the Debian build catches it.

## Existing decisions

Do not re-litigate these without being asked. [docs/ROADMAP.md](docs/ROADMAP.md) has the reasons.

- Qt6 + QML, not Electron, Tauri, or GTK4/libadwaita. No Kirigami.
- Callie's visual identity and design rules.
- No AI features, no telemetry, no Jot account or server.
- The editor edits Markdown text directly; it never round-trips through `QTextDocument`'s
  Markdown import and export.
- Shared tooling comes from greylocklabs/core, a submodule at `core/` that tracks core's `release`
  branch; the configs at the root are symlinks into `core/tooling/`.

## Build

Run `make` to list the commands; `make check` runs what CI runs. The Makefile sets the app's values
and includes core's `core/tooling/common.mk`, whose targets wrap CMake presets,
which work directly too: `cmake --workflow --preset ci` is CI's build, lint and test.
