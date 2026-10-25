# Sharing with Callie

Jot copies much of its code from [Callie](https://github.com/greylocklabs/callie) rather than
sharing it. The shared parts are moving into [greylocklabs/core](https://github.com/greylocklabs/core),
a submodule at `core/`; these rules keep that move mechanical.

Three parts have moved already:

- The tooling, in `core/tooling/`: the release, version check, notes, tarball and format scripts, the
  commit rules and hook, the format and lint configs, and the make targets in `common.mk`. The configs
  and the hook are symlinks into `core/tooling/`, `commitlint.config.mjs` passes Jot's scopes to core's
  rules, and the Makefile sets Jot's values before it includes `common.mk`.
- The built-in themes, in `core/themes/`. Their names and ids stay Callie's, so one theme file works
  in every app.
- The theme engine, in core's `greylock::core` library: `greylock::color`, `greylock::base16`,
  `greylock::ThemeSpec`, `greylock::ThemeLoader` and `greylock::ThemeLibrary`, with the built-in
  themes compiled in at `:/greylock/themes/`. `greylock::identity` gives Jot its id, folders and
  name from the values the top-level `CMakeLists.txt` sets before `add_subdirectory(core)`.

## Rules

- A shared file stays identical to Callie's copy. When one changes, make the same change in the
  other, in the same pull request series, or note why the two differ in the table below.
- App-specific values live in one place per file: the values above the `include` in the Makefile,
  and the scopes in `commitlint.config.mjs`.
- Shared code names nothing app-specific in comments or messages. Say "the app", not "Jot".
- Copied C++ keeps its structure and changes only the namespace (`callie` to `jot`), the
  include prefix (`callie/` to `jot/`) and the CMake prefix (`CALLIE_` to `JOT_`), so a diff
  between the two copies shows only real changes.

## Shared files

| File                                                             | State                                                                 |
| ---------------------------------------------------------------- | --------------------------------------------------------------------- |
| `package.json`, `package-lock.json`                              | Identical apart from the name and description                         |
| `CMakePresets.json`                                              | Same presets, `JOT_` cache variables                                  |
| `.github/workflows/ci.yml`                                       | Same jobs, Jot's dependencies, Ubuntu added                           |
| `.github/FUNDING.yml`                                            | Identical                                                             |
| `SECURITY.md`                                                    | Same text, Jot's names and scope                                      |
| `.github/ISSUE_TEMPLATE/`                                        | Neither app has one; both use the bug form in greylocklabs/.github    |
| `.github/workflows/release.yml`                                  | Callie's, without the OAuth secrets, and with an Ubuntu install check |
| `.github/workflows/code-review.yml`                              | Identical                                                             |
| `www/style.css`, `www/theme.js`, `www/carousel.js`, `www/fonts/` | Identical                                                             |
| `www/install.sh`                                                 | Renamed                                                               |
| `.github/review-bot/REPO_NOTES.md`                               | Same structure, Jot's stack and risks                                 |

Shared code, apart from what core holds (see `core/README.md`):

| Files                                                                                             | State                                                                                                                                                                                                                                          |
| ------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `Logging`, `LogFile`                                                                              | Renamed only, apart from comments that name what the app shows                                                                                                                                                                                 |
| `Paint`, `Palette`, `UpdateHint`, `FakeHttpServer`                                                | Renamed only                                                                                                                                                                                                                                   |
| `Upgrade`                                                                                         | Renamed, apart from the installer address it gives, which is Jot's at `jotwriter.org`                                                                                                                                                          |
| `Completion`                                                                                      | The engine only: commands' arguments and options' values come from the app through `Sources`, so Callie's calendar names move out of it                                                                                                        |
| `Diagnostics`                                                                                     | Versions, environment and log tails; Callie adds its accounts and sync state                                                                                                                                                                   |
| `ThemeController`                                                                                 | Calendar-only tokens and the calendar color functions removed, `titleBarHeight`, `badgeSize` and `editorFontFamily` added. `ThemeSpec` keeps the `[calendar]` section so theme files stay identical; Jot will use it for folder and tag colors |
| `ReleaseInfo`, `ThemesController`, `AboutDialog`, `UpdateDialog`, `WhatsNewDialog`, `ThemeEditor` | Renamed only, apart from the links, the About text and the theme editor saying "window" for "calendar"                                                                                                                                         |
| `SupportActions`                                                                                  | Jot's diagnostics are synchronous and have no accounts; it also opens the vault's folder                                                                                                                                                       |
| `SettingsForeign`                                                                                 | Callie keeps this in `EventModelForeign.h` beside its calendar types                                                                                                                                                                           |
| `KeyHelp`, `SettingsDialog`                                                                       | Callie's layout and sections, with Jot's shortcuts and settings                                                                                                                                                                                |
| `MonthModel`, `DatePicker`                                                                        | `MonthModel` keeps its own week helpers instead of Callie's `ViewRange`; `DatePicker` takes the first day of the week from the locale and shows a placeholder for no date                                                                      |

Repository settings match Callie's too: rebase merges only, auto-merge, branches deleted on
merge, the "Protect main branch" ruleset (with `build (ubuntu)` added to its required checks),
labels, Dependabot security updates and private vulnerability reporting.
