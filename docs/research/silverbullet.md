<!-- markdownlint-disable -->

# SilverBullet

Research snapshot from 2026-11-15, gathered to plan Jot. Versions and features drift; check sources before relying on them.

Facts come from the SilverBullet documentation, read from the `docs/` folder of the silverbulletmd/silverbullet `main` branch, and from silverbullet.md. The latest release was 2.12.1.

## 1. What it is

SilverBullet is a Markdown notes app built around a folder of `.md` files (a "space"). It started as a self-hosted web app (a PWA that keeps the whole space offline in the browser) and now also ships SilverBullet Desktop for macOS, Windows and Linux, which opens local folders and can sync with a Server. The Server and the `sb` CLI are MIT. Desktop is free for personal use, with a paid Pro tier (Meta Space). The backend was rewritten in Go (2.1) and then Rust (2.10); the client is TypeScript and CodeMirror 6.

Its pitch is a "malleable" notebook: almost every feature past the editor is written in Space Lua, a Lua dialect that lives in the space as Markdown pages, and users extend it the same way. It has moved toward teams since 2.11: accounts, roles, SSO, live sync, @mentions and a public read-only mode.

## 2. Feature inventory

**Editing**

- Live preview editor, inspired by Obsidian's. Alt-click reveals a formatted element's source, also on read-only pages. A command turns live preview off.
- Outlines: Alt-Up and Alt-Down move the item, section, table row or paragraph at the cursor. List items carry their children, a heading carries its section, and indenting a heading changes its level. Fold, unfold and fold all for list items.
- Slash commands, many of them templates: `/task`, `/comment`, `/hr`. A slash template is a page tagged `meta/template/slash` with `displayName`, `icon`, `aliases`, `group` and a `block` mode (insert as a new block, or convert the current line).
- Completion for `[[` links, `#` tags, `:` emoji, `[^` footnote labels, and front matter keys used elsewhere in the space.
- Footnotes, reference style `[^label]` and inline `^[text]`, drawn as a small marker that shows the body on hover and jumps to the definition on click. Undefined references are marked.
- Admonitions (callouts): `> **note** text`, with note, warning, danger and success, and custom kinds through CSS.
- Highlight, subscript and superscript.
- Image sizes in the alt text: `![Alt|300](a.png)`, `|x300`, `|300x200`, `|30%`.
- Vim mode with ex commands.
- Comments: HTML comments `<!-- ... -->` are drawn as a tinted block labelled "comment", with a Resolve button that deletes it. Their contents are still indexed, flagged `inComment`.
- Mobile keyboard bar with indent, move and format buttons.

**Linking**

- Wiki links `[[Page]]`, `[[Page#Header]]`, `[[Page@L12C3]]`, and relative Markdown links. Since 2.11, wiki links resolve by name anywhere in the space, as Obsidian does. When several notes share a name and none matches exactly, the link is shown as ambiguous and clicking it asks which one.
- `linkWriteFormat`: links SilverBullet writes are full paths, the shortest unique name, or the shortest unique path suffix.
- Anchors: `$name` on a paragraph, item, task or heading gives it a stable, space-wide name. `[[$name]]` links to it, `![[$name]]` embeds just that block, and `$` in the page picker searches them.
- Transclusion: `![[Page]]` or `![[Page#Header]]` embeds a page or a section, with a button to go to the source.
- Aspiring pages: every link to a page that does not exist is indexed, shown in the page picker with a "Create" chip, and can be listed as a to-write backlog.
- Linked Mentions (backlinks) with context, docked at the bottom of each page.
- Linked Tasks: open tasks anywhere that link to this page, docked at the top of it, tickable in place. A task in your 1:1 notes with A that mentions `[[B]]` shows up on B's page.
- Rename from the page title rewrites every link. Moving a page to another folder also moves documents only it links to.

**Structure and queries**

- Everything is indexed as objects: pages, tasks, list items, paragraphs, headings, table rows, links, tags, anchors. Each has a main tag (`page`, `task`, `item`) and user tags.
- Hashtags apply to the closest object: a tag alone in a paragraph tags the page, in a list item it tags the item, in a task the task, in a heading the heading.
- Inline attributes `[key: value]` (or `[key:: value]` for Obsidian and Logseq) on a page, item, task or paragraph. Values are YAML.
- Data blocks: a fenced block whose language is a hashtag (` ```#person `) holds YAML objects of that tag, several separated by `---`.
- Front matter has a form editor ("Live Object Edit") with typed fields and a date picker, and an Edit source switch. Keys: `tags`, `aliases`, `displayName`, `pageDecoration`.
- `tag.define` gives a tag a JSON Schema, an icon, a page decoration and a custom tag page. Objects that break their tag's schema get a lint warning. `schema.ref("person")` types a field as a link to a `#person` page.
- Space Lua Integrated Query (SLIQ): `${query[[from index.tasks() where not done order by due select ...]]}` written in a page and rendered live as a table or list. It has `group by`, `having`, aggregates, `limit` and `offset`.
- Workbench Data: a visual query builder (source tag, filters, group, order, table or list) that inserts or updates the query in the page. Its Schema tab lists every tag's attributes and types, with a count of objects that break the schema.
- Baked Sections: a query's current result written into the page as plain Markdown between `<!--#lua ... -->` and `<!--/lua-->` comments, so GitHub and other editors show a real table. Updated by hand, never on its own.
- Virtual pages: read-only pages computed from a name pattern. Clicking `#project` opens `tag:project`, which lists everything tagged `project`, grouped by pages, tasks, items and data.
- X-Ray: a lens that underlines everything the indexer extracted and shows its attributes on hover.
- Graph view: a local graph around the open page and a global page map, with filters by tag and relation kind.

**Tasks**

- `* [ ]` and `* [x]`, plus custom states `[IN PROGRESS]`, with a dropdown to pick one. States can be defined so they complete.
- No built-in due dates or repeats: dates are attributes like `[due: 2026-11-20]`, and lists come from queries.
- Journal: Journal: Today opens `Journal/YYYY-MM-DD` from a template. Previous and next day go to existing entries. A snippet in the docs makes the index page redirect to today's entry.

**Templates**

- Page templates: `suggestedName` with expressions (`Daily/${date.today()}`), `openIfExists`, a cursor marker `|^|`, front matter to apply, and an optional command and key binding so a template becomes its own command (Quick Note on `Ctrl-q q`).

**Navigation**

- Page picker (`Ctrl-k`), most recently used first, with `#tag` filtering, `$` for anchors, and Shift-Enter to create exactly what was typed. Fuzzy matching with several words and typo tolerance.
- Command palette (`Ctrl-/`), most recently run first, showing key bindings.
- Tag picker with counts. File tree with segments for pages, documents and tool pages; drag to move.
- Page decorations: front matter `pageDecoration` gives a page an icon, a prefix, CSS classes, a tree priority, or hides it. A tag can decorate all its pages, so every `#person` page gets the user icon.
- Movable views: the table of contents, linked mentions, linked tasks, mention inbox and graph can each dock in a sidebar, the bottom panel, above or below the page, or a modal, and remember where they were put.
- Key chords (`Ctrl-q j`, `Mod-. l`). Desktop has global shortcuts for any command.

**Search**

- Server-side full-text search with "all words" and "exact phrase" modes. Stemming for English, Dutch, German, Spanish and French, accent folding, and CJK word splitting.
- Optional semantic search with a local embedding model, and a Related Pages panel.

**Files**

- Any file can live in the space. Text files up to 5 MiB open in the editor with syntax colours. Images, audio, video and PDF open in viewers. Plugs add editors, such as Excalidraw.
- Upload by drag, paste or command; drag files from the tree into a page or to the file manager.

**History, sharing and sync**

- Revisions: git-backed history in Managed (SilverBullet commits after 30 seconds of quiet, at least every 5 minutes), Unmanaged (reads an existing repository, never commits) or Disabled mode. Page history with a colour diff and restore as one undo step; space history across all files. Commits are attributed to the account, "SilverBullet", or "External" for changes made by another editor or script.
- Git sync of that history to a remote, with a conflict list offering this side, the remote, both, or editing the markers by hand.
- Share: per page, front matter `share.uri` and `share.mode` (push, pull or sync) to a GitHub file, a Gist, a URL or another SilverBullet space. Push strips links to other pages, comments, and bakes queries, so the copy reads well elsewhere.
- Export: copy a page or selection as rich text or clean Markdown.
- Publish: a whole space can be public and read-only, served as a website with robots.txt and a sitemap.
- Live sync between devices and people within a few seconds, with a conflict widget.
- @mentions, a Mention Inbox, and signatures (`-- @name`) for comments and requests between people.
- Client encryption of the browser's offline copy, for untrusted devices. Not end-to-end.

**Extending**

- Space Lua: `space-lua` blocks on pages under `Library/` define commands, slash commands, widgets, event listeners, virtual pages and tags. `${expr}` anywhere renders a value or widget. Libraries are installed as pages, so their code can be read.
- Space Style: CSS in `space-style` blocks. Themes are pages tagged `meta/theme`.
- `sb` CLI for remote file operations, Lua, queries and screenshots, and an MCP server for AI assistants.

## 3. What Jot already has

Jot already covers a good part of this, often with a different syntax:

| SilverBullet                        | Jot                                                  |
| ----------------------------------- | ---------------------------------------------------- |
| Live preview, source reveal, toggle | Styled, Source and Read modes (#25, #28)             |
| Linked Mentions                     | Backlinks, with unlinked mentions too                |
| Rename rewrites links               | Links and `.assets` folders rewritten on rename      |
| Heading folding                     | Folding sections in Styled and Read (#522)           |
| Page templates, journal             | Templates and daily notes (#52)                      |
| Page picker, command palette        | Switcher with fuzzy matching, command palette (#31)  |
| Vim mode, ex commands               | Vi keys and a `:` command line (#32, #418)           |
| Tags, nested, tag picker            | Nested tags and a tag browser (#30)                  |
| Front matter key completion         | Front matter completion and warnings (#420, #421)    |
| Comments                            | CriticMarkup comments as margin notes (#53)          |
| Revisions, git sync                 | Note history with diffs (#48), git sync (#227, #370) |
| Global shortcut                     | Quick capture hotkey (#85)                           |
| Data blocks, schema lint            | Data files with JSON Schema checks (#211, #215)      |
| Notebook emoji                      | Emoji for notebooks (#120)                           |
| Mermaid via library                 | Mermaid built in (#57)                               |

Jot made two choices that change how SilverBullet's ideas carry over: it writes standard Markdown links rather than wikilinks (#185), and `@word` in a to-do is a marker (`@due`, `@p1`), so SilverBullet's `@mention` syntax is taken.

## 4. Context for existing issues

- **#59 Note types, #60 Properties panel.** `tag.define` is SilverBullet's version: a schema, an icon and a page decoration per tag, validated as lint warnings. Two details are worth taking. A field can be typed as a link to another type (`schema.ref("person")` for `owner: "[[Person/Alice]]"`), which is what Character and Location types need for relationships (#72). And a type can decorate every note of that type, so every Character shows the same icon in the note list and in links, without per-note front matter. Their form editor flips to YAML source with one menu item and shows the YAML whenever the cursor is inside it, which suits Jot's text-first editor.
- **#61 Saved views, #350 Search filters.** SilverBullet keeps the query in the note and offers a builder that writes it, then reopens it with "Edit in query builder". Their docs start people on the builder, not the language. Baked Sections answer a problem Jot's plain-files promise will meet: a live view shows nothing in other editors. Writing the current result into the note as a plain Markdown table between two comments, refreshed when asked, keeps a view readable on GitHub or in a phone's text editor. Their Schema tab, which lists a tag's attributes with the number of notes that break the schema, is a useful screen for #59 too.
- **#87 Plugin or script API.** SilverBullet is the strongest case for scripts living in the vault as readable pages, and for running only code under one folder (`Library/`). It also shows the cost: a long tail of breaking changes, a lint for "block that will not run", and a reload step. Anything Jot does here should start smaller, such as commands and views, and be versioned as #87 says.
- **#70 Codex mentions, #71 Mentions panel.** Linked Tasks is a cheap step toward this: on a character's note, list open to-dos anywhere that link to it. Their example is 1:1 meeting notes, and it fits worldbuilding to-dos like "check Mara's age in chapter 3".
- **#72 Relationships and graph.** Their graph has a local tab around the open note, expanded by clicking ghost nodes, and a global map that starts with only plain links and lets other relation kinds be switched on. Typed relations come from schema fields that are links.
- **#62 Modes and layouts.** Each SilverBullet panel (table of contents, backlinks, linked tasks, graph) can be docked in a sidebar, the bottom, above or below the note, or a popup, and remembers where it was put. That is a lighter way to get per-user layouts than fixed presets.
- **#280 Split panes.** No split panes in SilverBullet, but movable panels (above) cover some of the same need.
- **#449 Drawing note type, #84 Canvas.** SilverBullet does this through document editor plugs (Excalidraw) that open a file type in its own editor. A drawing is a file of its own, embedded in notes like an image.
- **#121 Locked notebook, #81 End-to-end encryption.** SilverBullet only encrypts the browser's offline copy, with a key derived by PBKDF2 and kept in memory. It says plainly that this is not end-to-end. Nothing to take, but it confirms that per-device encryption and E2EE are separate features.
- **#67 Named snapshots.** Their Revision: Create snapshot commits now rather than waiting, which is the same idea. They also mark history entries made outside the app as "External", which Jot's note history could show for edits from other editors.
- **#409 jot.nvim, #410 jot.el.** `sb` and the MCP endpoint let other tools work on a space. Jot's CLI already covers more of this locally, with `--json`.
- **#513 Website features.** Their features page groups by Editing, Links and queries, Customization, Sync, Platform, Desktop, each with a one-line description and a link to a doc page. It reads well and could be a model.

## 5. New ideas for Jot

Roughly in order of value for the effort.

1. **Move lines, list items and sections.** Alt+Up and Alt+Down move the list item at the cursor with its children, the section under a heading, a table row, or a paragraph. Shift+Alt+Left and Right could change a heading's level. A writer reordering scenes or a list of to-dos needs this every day.
2. **Draw footnotes.** Jot keeps footnotes but does not draw them. Show `[^1]` as a small marker with the body on hover, jump to the definition on click, mark references with no definition, and complete `[^` from labels in the note. Also read Pandoc's inline `^[text]`, since export goes through Pandoc already.
3. **Callouts.** GitHub's `> [!NOTE]`, `[!TIP]`, `[!IMPORTANT]`, `[!WARNING]` and `[!CAUTION]` rather than SilverBullet's own syntax, since GitHub and Obsidian draw those too.
4. **Linked To-dos.** On any note, the open to-dos elsewhere that link to it, tickable in place, above or beside the backlinks. It reuses the to-do index and backlinks Jot already has.
5. **Embed a note or a section.** `![](Note.md)` or `![](Note.md#Heading)`, drawn in place in Styled and Read modes, with a button to open the source. Standard Markdown image syntax keeps Jot's no-wikilinks rule; other tools show a broken image, which is a fair fallback. This also gives scrivenings (#65) and compile (#66) a building block.
6. **Missing notes list.** Links to notes that do not exist yet, gathered in one list as a to-write backlog, and offered as "Create" entries in the switcher. Worldbuilders link names before writing them.
7. **Slash commands.** `/` at the start of a line for headings, to-do, table, code block, callout, divider, date, and the vault's templates. Templates could take an optional `slash:` name. Discoverable for people who do not know Markdown, and quicker for those who do.
8. **Ask when a link could mean two notes.** Jot resolves a bare name to the shortest path silently. When two notes share a name and neither is in the linking note's folder, show the link as ambiguous and ask which one on click.
9. **Daily note navigation.** Previous and next day commands between existing daily notes, and a setting to open today's note at start.
10. **Image sizes.** Obsidian's `![alt|300](a.png)` is common in imported vaults. Reading it, and offering a resize handle that writes it, would help Obsidian imports (#91).
11. **Copy as rich text.** Copy a note or selection as formatted text for email or Google Docs, alongside Copy as Markdown. Jot already renders Read mode natively.
12. **Note icons from front matter.** Notebooks have emoji; notes could take an `icon:` key, and a note type (#59) could set one for all its notes.
13. **Stemming in search.** Jot's FTS5 index folds accents but has no stemming, so `bikes` misses `bike`. FTS5's `porter` tokenizer wraps `unicode61` for English; other languages would need more.
14. **Global shortcuts for any command.** Jot binds one global shortcut, quick capture. The XDG GlobalShortcuts portal could take any command, such as "open today's note".
15. **Publish a note to a Gist or a GitHub file.** Push mode with links to other notes made plain text and comments removed. Lower priority, but it fits #126 and needs no server of Jot's own.

## 6. Not for Jot

- **Accounts, roles, SSO, live collaboration, @mention inboxes, public spaces.** These need a server and a Jot account, which the roadmap rules out.
- **Semantic search, MCP server, "agents" in the CLI.** Ruled out by "No AI" in Jot's promises.
- **Space Lua as the core.** SilverBullet builds its own features in Lua; Jot's are C++ and QML by design. Scripting stays an add-on (#87).
- **Wiki link resolution by name and `[[...]]` syntax.** Jot settled on standard Markdown links (#185).
- **Inline `[key: value]` attributes and `$anchors` in prose.** They are SilverBullet-only syntax that other tools show as literal text. If Jot ever needs block references, a heading or an HTML anchor is more portable.
- **Custom task states such as `[IN PROGRESS]`.** Other Markdown tools do not read them as to-dos at all.
- **Tool pages under `Library/`, Space Style, Meta Space.** Jot's settings and themes live in its own files, which is simpler for a desktop app.

## Sources

- https://silverbullet.md/ and https://silverbullet.md/features
- https://docs.silverbullet.md/ and its CHANGELOG
- https://github.com/silverbulletmd/silverbullet, `docs/` on `main`
