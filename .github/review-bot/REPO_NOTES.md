# Repo notes - Jot

Facts the review bot cannot infer from one pass over the diff. The philosophy, the finding
categories, the workflow, the scoring, and the output format all live in the bot's base prompt; this
file only refines them for this repo.

## The stack

Get this right before judging anything. **This is not a web app and there is no JavaScript
runtime.**

| Layer | What it is                                                     | Where       |
| ----- | -------------------------------------------------------------- | ----------- |
| Core  | C++20, Qt 6. Vault, front matter, index, search, sync. No QML. | `src/core/` |
| UI    | QML module `Jot.Ui`. Declarative views over C++ models.        | `src/ui/`   |
| App   | `jot-gui`, a `main.cpp` that loads `Jot.Ui`.                   | `src/gui/`  |
| CLI   | `jot`, a `QCommandLineParser` binary over the same core.       | `src/cli/`  |
| Docs  | Roadmap, suite rules and research. Planning, not product.      | `docs/`     |

The JavaScript inside `.qml` files is QML's expression language, not browser JS. There is no DOM, no
`window`, no npm dependency in the shipped product. The only Node in the repo is developer tooling
(commitlint, prettier, markdownlint).

## The normative documents

**Read these before reviewing, and treat them as binding.** A diff that contradicts them is a
finding even when the code is otherwise correct, and the finding should quote the rule it breaks.

- `GUIDELINES.md` - repository layout, conventions, and the list of settled decisions.
- `CONTRIBUTING.md` - commit message rules and the comment standard.
- `docs/SUITE.md` - which files must stay identical to Callie's.
- `docs/ROADMAP.md` - scope and the reasons behind the settled decisions.

The decisions in `GUIDELINES.md` under "Existing decisions" are closed. Do not suggest Electron,
Tauri, GTK, libadwaita, Kirigami, AI features, telemetry, or a Jot account or server.

Two rules worth checking on every diff because nothing automated catches them:

- **No em dashes** anywhere in comments, documentation, or commit messages. A colon, a comma, or a
  second sentence instead.
- **Comments are one or two sentences and explain why, not what.** A comment that restates the code,
  or runs to a paragraph, is a finding. `TODO` entries use the `TODO(scope):` form.

## This project is early, so calibrate

Jot is pre-release and mostly scaffold. **Do not report missing features as findings.** The window
is empty, the CLI only has `--help` and `--version`, and there is no vault, editor or sync yet.
Judge the code that is there. An absent subsystem is on the roadmap, not a defect.

## What CI already covers, so never mention it

On every PR: `clang-format` and `qmlformat` via `core/tooling/format.sh check`, `qmllint` over the whole
QML module, Fedora, Debian and Ubuntu builds with `-Wall -Wextra -Wpedantic -Werror`, `ctest`,
`markdownlint`, `prettier --check`, and `commitlint`. Formatting, compiler warnings, lint rules and
commit message shape are not your findings.

## What CI cannot cover, so it is yours

**Never losing a user's text is the highest-value dimension here.** Notes are the user's own files,
and a notes app that silently drops or rewrites text is worthless. For every diff touching note
reading, writing, import or sync:

- writes: does the file go through `QSaveFile` or an equivalent temp-and-rename, so a crash cannot
  leave a half-written note;
- fidelity: does a load-and-save round trip keep the bytes the user wrote, including line endings,
  trailing whitespace, unknown front matter keys and YAML formatting;
- conflicts: when two versions disagree, is one kept as a conflict copy rather than overwritten, and
  does an edit on one side win over a delete on the other;
- paths: can an imported or synced name escape the vault (`..`, absolute paths, symlinks), and are
  names valid on every target;
- encoding: is text read and written as UTF-8 explicitly, never through the locale.

Also yours: the C++ and QML boundary, model correctness, and credential handling.

## Seams

- **Plain files are the source of truth.** Anything stored beside them (index, search, sync state)
  must be rebuildable from the files. A change that puts note data only in SQLite is a finding.
- **The editor edits Markdown text.** Routing note text through `QTextDocument::setMarkdown` or
  `toMarkdown` is a blocking finding: that round trip rewrites the user's syntax.
- **Layering.** `src/core/` must not gain a QML or QtQuick dependency: the CLI links it. QML types
  belong in `src/ui/`.
- **The `Theme` singleton is the only source of visual values.** A literal colour, spacing number,
  radius or animation duration in a `.qml` file is a finding. Theme-driven values come from
  `core/themes/*.toml`; spacing, type sizes and metrics are constants in `ThemeController`.
- **Themes are data.** Theme files must never be able to run code or load QML.
- **Text arithmetic belongs in C++.** Parsing, offsets and layout maths inside a `.qml` file is a
  finding; the model should expose it instead.
- **Credentials.** OAuth clients and sync keys never land in a committed file, a compile definition,
  a log line, or an error message. Tokens belong in the system keyring. OAuth uses PKCE with a
  loopback redirect bound to `QHostAddress::LocalHost`.
- **Network code.** `QNetworkAccessManager` replies must handle the error case, and nothing on the
  sync path may block the UI thread.
- **Suite files.** A change to a file `docs/SUITE.md` lists as identical to Callie's should say
  whether Callie gets the same change.

## Coverage matrix

- New or changed logic in `src/core/` needs a Qt Test case in `tests/`.
- A bug fix that lost or changed note text needs a regression test that would have failed before
  the fix. Say so explicitly when one is missing.
- A new QML component does not need a test, but it does need to survive `qmllint`.
