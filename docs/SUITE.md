# Sharing with Callie

Jot copies its tooling and much of its code from [Callie](https://github.com/greylocklabs/callie)
rather than sharing it. When a third app arrives, the shared parts will move into one place that
all the apps use. These rules keep that move mechanical.

## Rules

- A shared file stays identical to Callie's copy. When one changes, make the same change in the
  other, in the same pull request series, or note why the two differ in the table below.
- App-specific values live in one marked place per file: the block above the "Shared by every app
  in the suite" line in the Makefile, and the `scopes` list at the top of
  `commitlint.config.mjs`.
- Shared code names nothing app-specific in comments or messages. Say "the app", not "Jot".
- Copied C++ keeps its structure and changes only the namespace (`callie` to `jot`), the
  include prefix (`callie/` to `jot/`) and the CMake prefix (`CALLIE_` to `JOT_`), so a diff
  between the two copies shows only real changes.

## Shared files

| File                                                                          | State                                                                                                  |
| ----------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------ |
| `.clang-format`, `.qmlformat.ini`, `.qmllint.ini`, `.editorconfig`, `.clangd` | Identical                                                                                              |
| `.prettierrc`, `.prettierignore`, `.markdownlint-cli2.yaml`                   | Identical                                                                                              |
| `.githooks/commit-msg`, `scripts/format.sh`                                   | Identical                                                                                              |
| `cliff.toml`                                                                  | Identical apart from "the app" in one comment                                                          |
| `package.json`, `package-lock.json`                                           | Identical apart from the name and description                                                          |
| `commitlint.config.mjs`                                                       | Shared rules, app scopes at the top                                                                    |
| `Makefile`                                                                    | Shared targets, app block at the top. No `logs` target until the CLI has `logs`                        |
| `CMakePresets.json`                                                           | Same presets, `JOT_` cache variables                                                                   |
| `.github/workflows/ci.yml`                                                    | Same jobs, Jot's dependencies, Ubuntu added                                                            |
| `.github/FUNDING.yml`, `.github/ISSUE_TEMPLATE/config.yml`                    | Identical                                                                                              |
| `.github/ISSUE_TEMPLATE/bug_report.yml`, `SECURITY.md`                        | Same text, Jot's names and scope                                                                       |
| `scripts/check-version.sh`, `scripts/release.sh`, `scripts/release-notes.sh`  | App-agnostic: they read the app's name and id from `CMakeLists.txt`, so they can be shared as they are |
| `.github/workflows/release.yml`                                               | Callie's, without the OAuth secrets, and with an Ubuntu install check                                  |
| `.github/workflows/code-review.yml`                                           | Identical                                                                                              |
| `www/style.css`, `www/theme.js`, `www/carousel.js`, `www/fonts/`              | Identical                                                                                              |
| `www/install.sh`                                                              | Renamed                                                                                                |
| `.github/review-bot/REPO_NOTES.md`                                            | Same structure, Jot's stack and risks                                                                  |

Shared code:

| Files                                                                                                             | State                                                                                                                                                                                                                                          |
| ----------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `themes/*.toml`                                                                                                   | Identical. The theme names and ids stay Callie's, so one theme file works in every app                                                                                                                                                         |
| `Color`, `Base16`, `ThemeLibrary`, `Identity`, `Logging`, `LogFile`                                               | Renamed only, apart from comments that name what the app shows                                                                                                                                                                                 |
| `ThemeSpec`, `ThemeLoader`                                                                                        | Renamed, plus an optional `[editor]` section: the font notes are written in and code colors. Callie should take it so a theme file with one loads there without warnings                                                                       |
| `Paint`, `Palette`, `UpdateHint`, `UpdateCheck`, `FakeHttpServer`                                                 | Renamed only                                                                                                                                                                                                                                   |
| `Upgrade`                                                                                                         | Renamed, apart from the installer address it gives, which is Jot's at `jotwriter.org`                                                                                                                                                          |
| `Releases`                                                                                                        | Accepts Ubuntu 26.04 and later, which Callie does not yet                                                                                                                                                                                      |
| `Completion`                                                                                                      | The engine only: commands' arguments and options' values come from the app through `Sources`, so Callie's calendar names move out of it                                                                                                        |
| `Diagnostics`                                                                                                     | Versions, environment and log tails; Callie adds its accounts and sync state                                                                                                                                                                   |
| `ThemeController`                                                                                                 | Calendar-only tokens and the calendar color functions removed, `titleBarHeight`, `badgeSize` and `editorFontFamily` added. `ThemeSpec` keeps the `[calendar]` section so theme files stay identical; Jot will use it for folder and tag colors |
| `ReleaseNotes`, `ReleaseInfo`, `ThemesController`, `AboutDialog`, `UpdateDialog`, `WhatsNewDialog`, `ThemeEditor` | Renamed only, apart from the links, the About text and the theme editor saying "window" for "calendar"                                                                                                                                         |
| `SupportActions`                                                                                                  | Jot's diagnostics are synchronous and have no accounts; it also opens the vault's folder                                                                                                                                                       |
| `SettingsForeign`                                                                                                 | Callie keeps this in `EventModelForeign.h` beside its calendar types                                                                                                                                                                           |
| `KeyHelp`, `SettingsDialog`                                                                                       | Callie's layout and sections, with Jot's shortcuts and settings                                                                                                                                                                                |
| `MonthModel`, `DatePicker`                                                                                        | `MonthModel` keeps its own week helpers instead of Callie's `ViewRange`; `DatePicker` takes the first day of the week from the locale and shows a placeholder for no date                                                                      |

`.markdownlint-cli2.yaml` and `.prettierignore` also skip `tests/data/`, which holds sample notes
here; Callie should take the same lines. The Makefile's `screenshots` recipe now reads
`SHOT_PREPARE` and `SHOT_APP` from the app block, which Callie's needs to adopt to stay shared.

Callie's `commitlint.config.mjs`, `Makefile`, `cliff.toml` and release scripts still use the older layout. Moving
them to this one is the first step of extraction.

Repository settings match Callie's too: rebase merges only, auto-merge, branches deleted on
merge, the "Protect main branch" ruleset (with `build (ubuntu)` added to its required checks),
labels, Dependabot security updates and private vulnerability reporting.
