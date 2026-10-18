# Contributing to Jot

## Commit messages

Jot uses [Conventional Commits](https://www.conventionalcommits.org), enforced by
commitlint in CI and optionally by a local hook.

```text
<type>(<scope>): <subject>

<body>

<footer>
```

Types: `build`, `chore`, `ci`, `docs`, `feat`, `fix`, `perf`, `refactor`, `revert`,
`style`, `test`.

Scopes: `core`, `gui`, `qml`, `editor`, `theme`, `cli`, `sync`, `import`, `packaging`, `data`,
`site`, `ci`, `deps`, and `deps-dev` for Dependabot's updates to development dependencies. Omit
the scope for genuinely cross-cutting changes.

Rules the linter enforces:

- Subject is lower case, imperative mood, no trailing period.
- Header is at most 72 characters.
- Body lines wrap at 100 characters, with a blank line before the body.

Good:

```text
feat(editor): dim markdown markers outside the cursor's block
fix(core): keep front matter when a note has windows line endings
docs(theme): explain why tokens live in a singleton
```

Bad:

```text
Updated stuff.
feat(qml): Added drag and drop to the notebook tree, which required changes to NoteList
fix: bug
```

Write the body when the change needs a reason. Explain why, not what. The diff already
says what.

### Local hook

```sh
npm install
git config core.hooksPath .githooks
```

The hook exits silently if `node_modules` is missing, so contributors who do not want a
Node toolchain can rely on CI instead.

## Comments

Comments are for the reader who understands the language but not the decision. Keep them
short and explain intent.

- Use `///` on public types and functions in headers, and on the root of every QML
  component. Say what it is for and what it deliberately does not do.
- Use `//` inline, sparingly, to explain why something non-obvious is the way it is.
- One or two sentences. If a comment needs a paragraph, either the code should be
  restructured or the explanation belongs in a document.
- Sentence case, ending with a period.
- No em dashes. Use a colon, a comma, or a second sentence.
- Do not restate the code. `// increment i` earns nothing.
- Never commit commented-out code. Git remembers it.
- Wrap comments at the same 100 column limit as code.

`TODO` format, so they can be found and retired:

```cpp
// TODO(sync): drop once transports report their own capabilities.
```

Good:

```cpp
/// Front matter is a `---` line at the very start of the note, closed by a `---` or `...`
/// line. Anything else, including an unclosed fence, is all body, so no text is lost.

// Re-hash only when size, mtime or inode changed; hashing every note on startup is slow.
```

Bad:

```cpp
// This function splits the note into parts - it is very important for the
// editor to work correctly and is called by the NoteModel class whenever a
// note is opened.

// loop over the notes
```

## Code style

`clang-format`, `qmlformat` and `.editorconfig` handle formatting. One script drives the first
two so that CMake, CI and a local run agree:

```sh
scripts/format.sh fix      # rewrite
scripts/format.sh check    # verify, as CI does
```

Markdown, JSON and YAML are formatted by prettier and linted by markdownlint:

```sh
npm run format:md
npm run lint:md
```

## Tests

Qt Test cases live in `tests/`, one executable per file, registered with ctest.

```sh
ctest --preset dev
```

New logic in `src/core/` needs a test. Anything asserting on wall-clock values must pin `TZ`, as
`tests/CMakeLists.txt` does.

Beyond formatting:

- Every visual value in QML reads from the `Theme` singleton, whose values come from the theme
  files in `themes/`. Do not hard-code colors, spacing, radii, or durations. Callie's
  [DESIGN.md](https://github.com/greylocklabs/callie/blob/main/DESIGN.md) explains what each token is
  for until Jot has its own.
- To work on a theme, run `make gallery THEME=path/to/theme.toml`. It shows every token and
  component, and updates as you save the file. `make watch` reloads the app's QML on save, and
  `make screenshots` renders the app and the gallery in every built-in theme, as CI does.
- Keep text and layout arithmetic in C++ models. QML positions things, it does not compute them.
- Do not add Kirigami or any dependency that imports another desktop's visual language.
