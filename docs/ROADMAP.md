# Jot roadmap

Jot is a Linux-first notes app built with Qt 6 and QML. Its first job is to replace Joplin for
everyday Markdown notes. Its second job is to be the best Linux home for long-form writers and
for people building fictional worlds and games, with first-class support for characters, places
and other story entities.

Jot is the second app in a small suite with Callie (and later a task app). The code lives at
`github.com/greylocklabs/jot`, the app id is `org.callieapp.Jot`, and the website is
<https://jotwriter.org>. The first packages are .deb and .rpm for Fedora,
Debian and Ubuntu.

Background research lives in [docs/research/](research/):

- [callie-reuse.md](research/callie-reuse.md): what to take from Callie
- [joplin.md](research/joplin.md): Joplin and open-source peers (QOwnNotes, Marknote, Ghostwriter, Logseq)
- [commercial-apps.md](research/commercial-apps.md): Obsidian, Notion, Bear, iA Writer, Typora, Capacities, Tana
- [writers-and-worldbuilding.md](research/writers-and-worldbuilding.md): Scrivener, novelWriter, Fountain, articy, Ink
- [technical.md](research/technical.md): sync engine, backend APIs, Markdown editing in QML, search, packaging

## Positioning

The market gap is clear: a fast, native, good-looking app that keeps notes as plain files and
treats sync as optional. Joplin is open and syncs everywhere, but it runs on Electron, keeps notes
in SQLite under hex ids, and its UI is dated. Obsidian has the right model (a folder of Markdown
files) but is proprietary Electron and charges for sync. QOwnNotes is native and file-based, but
dense, keeps tags outside the files, and leaves sync to others. Writers on Linux have no
Scrivener, and the worldbuilding tools (Campfire, World Anvil, LegendKeeper) are cloud services.

Jot's promises:

1. **Your notes are files.** A vault is a folder of `.md` files with YAML front matter. Folders
   are notebooks. Tags, tasks and properties live in the files. Every database Jot keeps is a
   cache that can be deleted and rebuilt.
2. **Native and fast.** Cold start and search should be quick enough to measure and publish,
   for example a 10,000-note vault.
3. **Sync is yours to choose.** No sync, a folder that Syncthing or Nextcloud already syncs, S3,
   Dropbox or Google Drive. No Jot account and no Jot server.
4. **No AI, no telemetry, no subscription.** This carries over Callie's non-goals and is a
   selling point given what Notion, Evernote, Tana and others have done.
5. **Structure is opt-in.** You can write without first setting up types, templates or projects.
   The writer and worldbuilding tools grow out of the same plain files.

## Settled decisions

These follow Callie's GUIDELINES.md and DESIGN.md unless noted.

- **Stack:** C++20, Qt 6.8 minimum, CMake with presets, Ninja, QML with Basic controls, no
  Kirigami. Dependencies come from distro packages via `find_package`.
- **Layers:** the same layering as Callie:
  - `jotcore`: a static library with no QML.
  - `Jot.Ui`: the QML module plus controllers.
  - `jot-gui`: the desktop app.
  - `jot`: the CLI, built from a library so tests can drive it.
  - `jot-gallery`: a component and theme viewer.
- **Design language:** Callie's theme system, palette, fonts (Nunito and Fraunces), radii,
  motion, sticker buttons, frameless window and design rules. Dark first, light equal. Pink is
  reserved for a fixed set of uses. The editor needs one added font role ([#27](https://github.com/greylocklabs/jot/issues/27)).
- **License:** MIT, matching Callie. Never copy Joplin source (AGPL); write Joplin import from its
  documented formats.
- **Storage:** plain files are the source of truth. SQLite holds the index, search and sync state.
- **Editor:** the Markdown text in a QML TextEdit is the source of truth. Live preview comes from
  a QSyntaxHighlighter, never from round-tripping through `QTextDocument::setMarkdown`. Marknote
  is rewriting its editor because that round trip was lossy.
- **Sync engine:** Jot's own, Unison-style three-way engine over thin transports. Not rclone
  bisync and not the Joplin sync format.
- **Names:** repo `greylocklabs/jot`, app id `org.callieapp.Jot`, binaries `jot-gui` and `jot`.
  Debian's `athena-jot` package may also install `/usr/bin/jot`. If it does, the .deb declares
  `Conflicts: athena-jot`.
- **Platforms:** Fedora 44+, Debian 13+ and Ubuntu 26.04+ on x86_64. All three ship Qt 6.8 or
  newer. Ubuntu 24.04 (Qt 6.4) waits for Flatpak.
- **Packaging:** hand-written rpm spec and debian/ directory, built and smoke-tested in containers
  in CI, plus an installer at `jotwriter.org/install.sh`.
- **Website:** jotwriter.org, from `www/` in this repo, with its landing page, privacy and terms
  pages adapted from Callie's, and the installer. Its stylesheet, scripts and fonts are copies of
  Callie's site files, kept identical.

## Reuse from Callie

Code is copied from Callie, not shared, for now. Copying is set up so that a shared "kit" can be
extracted once a third app (tasks) arrives. [SUITE.md](SUITE.md) has the rules: shared files stay
identical to Callie's, and app-specific values sit in one marked place per file.

| Copy nearly as-is                                                                                                                                                                                                | Adapt                                          | Leave behind                             |
| ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------- | ---------------------------------------- |
| CMake skeleton, presets, `Identity` devel mode, LiveQml, gallery                                                                                                                                                 | Settings (notes options)                       | Event, ICS, recurrence, CalDAV, Graph    |
| Makefile, `format.sh`, `check-version.sh`, `release-notes.sh`                                                                                                                                                    | commitlint scopes                              | Calendar views and models                |
| `.clang-format`, `.qmlformat.ini`, `.qmllint.ini`, prettier, markdownlint, `.editorconfig`, `.gitattributes`, git hook                                                                                           | Theme TOMLs: drop `[calendar]`, add `[editor]` | `[calendar]` theme section, grid metrics |
| ThemeLoader, ThemeLibrary, Base16, Color (OKLCH, contrast), ThemeController, ThemeEditor                                                                                                                         | `QueuedSource` pattern applied to sync         | QuickAdd, Places, Contacts               |
| Theme files (Callie, Callie Light, Paper, Minimal, Tomorrow)                                                                                                                                                     | CLI commands                                   |                                          |
| TokenStore and Keychain, GoogleAuth, PKCE loopback, SignInPage, credential injection CMake                                                                                                                       | `doctor`, `status`                             |                                          |
| AccountStore and AccountManager ordering, Outcome, Logging, Diagnostics                                                                                                                                          |                                                |                                          |
| UpdateCheck, Releases, `upgrade`/`downgrade`, SingleInstance, TrayItem, Autostart, KeyRouter (vi keys, WhichKey)                                                                                                 |                                                |                                          |
| QML: StickerButton, PillButton, DangerButton, Toggle, Dropdown, Field, FormRow, MenuCard, Tip, Scrim, LazyPopup, SuggestionPopup, UndoToast, Glyph, WindowControls, ResizeFrame, About, WhatsNew, Update dialogs |                                                |                                          |
| CLI Paint and Palette, embedded completions, `--json` conventions                                                                                                                                                |                                                |                                          |
| CI workflows (ci, release, pages, review-bot), `package_smoke.sh`, `install.sh` with tests, rpm and deb skeletons                                                                                                |                                                |                                          |
| `www/` style.css, theme.js, carousel, privacy and terms pages                                                                                                                                                    |                                                |                                          |

## Vault format

[FORMAT.md](FORMAT.md) defines it. In short: folders are notebooks, notes are `.md` files whose
file name is the title, front matter is optional YAML that Jot edits key by key, links are standard
relative Markdown links that Jot rewrites on rename, and attachments go in a `Note.assets/` folder
beside each note. Manual order and folder appearance live in `.jot/layout.yaml`, and Jot's index
and sync state stay outside the vault.

## Milestones

Each milestone ends in a release that is useful on its own. Versions follow Callie (0.x until
the format and sync are trusted). The work itself is tracked as issues under each
[GitHub milestone](https://github.com/greylocklabs/jot/milestones); this section keeps only the order
and the goal of each.

- **Foundation:** tooling, CI, packaging, themes and window chrome from Callie. Done; its loose
  ends (the website and the final vault format) are in 0.1.
- **[0.1: A notes app I can live in](https://github.com/greylocklabs/jot/milestone/1):** Lara can
  move off Joplin for local use. Vault and notebooks, the live-preview editor, links and
  backlinks, tags, tasks, search, outside edits, Joplin import, the CLI and the website.
- **[0.2: Sync](https://github.com/greylocklabs/jot/milestone/2):** the same vault on a laptop and a
  desktop through a cloud the user picks. Jot's own three-way engine, syncing with a folder,
  conflict handling, note history and quick capture in 0.2.0; S3, Dropbox and Google Drive
  follow as 0.2.x releases as each lands.
- **[0.3: Data files](https://github.com/greylocklabs/jot/milestone/11):** JSON, CSV and YAML in
  the vault, edited as tables or as text, and checked against a schema.
- **[0.5: Writer basics](https://github.com/greylocklabs/jot/milestone/3):** pleasant for daily
  writing, not just notes. Focus mode, counts and targets, templates, export, spellcheck, math,
  diagrams and audio.
- **[0.6: Types, properties and views](https://github.com/greylocklabs/jot/milestone/4):** note types,
  typed properties and saved views, the foundation for everything after, and presets of them for
  note takers, game masters, game developers and novelists.
- **[0.7: Writing projects](https://github.com/greylocklabs/jot/milestone/5):** binder, corkboard,
  scrivenings, compile, snapshots, Fountain, and Scrivener import.
- **[0.8: Worldbuilding and characters](https://github.com/greylocklabs/jot/milestone/6):** codex
  mentions, relationships, timelines, progressions and image maps.
- **[0.9: Game narrative](https://github.com/greylocklabs/jot/milestone/7):** Ink, Yarn and Twine
  files, game design documents, and JSON export.
- **[Later](https://github.com/greylocklabs/jot/milestone/8):** encryption, more transports, Flatpak,
  plugins, capture by email and hotkey, and mobile.

Research passes on other apps carry the `research` label and land in [docs/research/](research/).

## Risks

- **Editor quality decides everything.** A live-preview editor that jumps, lags or breaks IME
  will sink the app. Build a long-document test note and an IME smoke test early. Keep source
  mode as the escape hatch. Defer a block-based Typora-style editor until the highlighter
  approach has clearly run out.
- **Sync correctness.** Write the engine against fake transports with randomized two-device
  scenarios before any real backend. Never ship a sync path that can lose a byte.
- **Google verification.** `drive.file` avoids the security assessment, but the consent screen
  still needs brand verification for name and logo. Start it early, as for Callie.
- **Dropbox app review.** Development apps are capped at a small number of users until
  production approval.
- **Scope.** Milestones 4 to 7 are the differentiator but also the scope trap. Each must reuse
  the one index and the plain-file format. Avoid real-time collaboration, a home-grown
  typesetter, a flow-graph editor and map tiles.
- **Distro Qt floor.** Ubuntu 24.04 ships Qt 6.4 and cannot be supported natively. Point those
  users at Flatpak later.
- **Shared site.** Jot's pages deploy with Callie's, so a broken Jot page blocks Callie's site
  deploy and the other way round. Keep Jot's pages self-contained under `www/jot/`.

## Open questions

1. Ubuntu support in Callie's installer, which refuses Ubuntu today. Jot's installer will accept
   26.04, so the shared installer logic should too.
