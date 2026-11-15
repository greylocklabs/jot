<!-- markdownlint-disable -->

# Logseq

Research snapshot from 2026-11-15, gathered to plan Jot. Versions and features drift; check sources before relying on them.

Facts about the DB version come from `db-version.md` and `db-version-changes.md` in the logseq/docs repository (describing the app as of 2026-04-28), the 2.0.2 release notes, and the DB changelog on the Logseq forum. Facts about the file version come from the docs graph's pages. This extends the short Logseq note in [joplin.md](joplin.md).

## 1. What it is

Logseq is an outliner: every paragraph is a block in a tree, pages are trees of blocks, and a daily journal is the default place to write. It is AGPL and built in ClojureScript on Electron, with Capacitor mobile apps.

It is in the middle of a split:

- **File graphs ("OG")**: a folder of Markdown or Org files, with Logseq's own syntax for block properties, ids and task markers. Split off to the logseq/og repository and in maintenance. The last release, 0.10.15, was 2025-12-01.
- **DB graphs**: 2.0 beta (2.0.1 on 2026-07-13, 2.0.2 on 2026-10-07). The graph is a SQLite database under `~/logseq/graphs/NAME/db.sqlite`, with assets beside it. Markdown is no longer the source of truth. A "Markdown Mirror" writes the graph to disk as plain Markdown as you edit, but it is read-only: as of the May 2026 changelog, two-way sync "is in development on a feature branch". Org mode, whiteboards, slides, Excalidraw drawing and the built-in Zotero integration are gone, some meant to return as plugins.

Sync (with real-time collaboration) and Publish are paid, through OpenCollective sponsorship at $5 or $15 a month, and both can be self-hosted. Sync is end-to-end encrypted with a password set per user.

The split is the main lesson for Jot. Logseq's users chose it for plain files, and moving the source of truth into SQLite cost it trust: forum threads ask whether to stay on OG indefinitely, and users say they will wait for two-way Markdown before moving.

## 2. Feature inventory

**Blocks and the outliner**

- Every line is a block with a bullet. Tab and Shift+Tab indent and outdent with children; Alt+Shift+Up and Down move a block with its children. A setting picks "logical" outdenting (Roam's) or the usual kind.
- Click a bullet to zoom into a block: it becomes the page, with a breadcrumb back. Blocks collapse with an arrow.
- Shift-click a link opens it in the right sidebar, which stacks several pages and blocks to read or edit beside the main one.
- Block references `((uuid))` show another block's text inline and live; block embeds show it with its children, editable in place. Page embeds do the same for a whole page. In the DB version blocks and pages are both "nodes" and both use `[[...]]`.
- Moving blocks to another page with a command ("Move blocks to"), and multi-select of blocks with Shift+arrows for bulk tag, property, copy and delete.
- Enhanced paste: HTML is converted, pasted Logseq blocks keep their nesting, a URL pasted over selected text makes a link, YouTube URLs become embeds. Shift+Ctrl+V pastes raw text.

**Links**

- `[[Page]]` links and `#tag` (a tag is a page). Linking a page that does not exist creates it.
- Linked References at the foot of each page, grouped by page, with a filter: click a co-occurring page or tag to show only blocks mentioning it, Shift-click to hide them. Filters are remembered per page.
- Unlinked References: pages that mention the title as plain text, with one click to link them.
- Aliases (`alias::` property). Namespaces `[[a/b/c]]` make a hierarchy; the DB version shows it as a collapsible tree above the page, and a Library page edits it as an outline.
- Date links in natural language: `[[Today]]`, `[[Next Friday]]` complete to that day's journal page. A date picker takes `next week`.

**Journals**

- The Journals view is a single scrolling page of daily notes, newest first, today's created automatically.
- `g n` and `g p` go to the next and previous day. Scheduled and deadline tasks show on today's journal.
- Quick add: a popup that appends to today's journal without leaving what you were doing. On mobile, a Capture tab and the system Share sheet do the same.
- `logseq://x-callback-url/quickCapture?url=&title=&content=` captures from a browser bookmarklet.

**Properties, tags and types (DB version)**

- Properties on any block or page, typed: Text, Number, Date, DateTime, Checkbox, Url, Node. A Node property can be limited to nodes with a tag. Properties can have choices (with icons), a default value, multiple values, a UI position (before the text, after it, below it), and be hidden by default or when empty.
- Tags are types ("supertags"). A tag has Tag Properties that every tagged node shows and fills in. Tags extend parent tags (`#Audiobook` extends `#Book` and `#MediaObject`) and inherit their properties. Page names are unique per tag, so `Apple #Company` and `Apple #Fruit` can both exist.
- Bidirectional properties: with it on, a `#Book` whose `author` is Ada shows a `Books` list on Ada's page.
- Keyboard: `Ctrl+Alt+P` adds a property, `Cmd-J` jumps to a property by letter hint, `p t` tags, `p i` sets an icon.
- In the file version, properties are `key:: value` lines at the top of a block, and page properties live in the page's first block.

**Tasks**

- File version: `TODO`/`DOING`/`DONE` or `NOW`/`LATER` markers, `[#A]` priorities, `SCHEDULED: <2026-11-20 Fri>` and `DEADLINE:` lines, repeaters like `.+1w`, and a LOGBOOK of clocked time.
- DB version: a task is a block tagged `#Task` with Status (Backlog, Todo, Doing, In Review, Done, Canceled, plus your own), Priority, Deadline and Scheduled. `Cmd-Enter` cycles status. Status history replaces the logbook and gives time spent. Repeats work on any date property paired with a checkbox-like property. Custom task types extend `#Task`.
- 2.0.2 fixed monthly repeats drifting after a short month and repeats advancing in UTC rather than the local calendar.

**Queries and views**

- Simple queries `{{query (and (task todo) [[project]])}}` and advanced queries in Datalog. A query builder writes them.
- DB version views: Table (spreadsheet-like, editable cells, multi-column sort, pinned and resized columns), List (an outline grouped by page) and Gallery (cards, with a cover from an asset or a URL property). Filters with typed operators, match all or any, group by a property, several saved views per tag page. Every tag page is a table of its nodes, with "+ New".

**Other features**

- Templates are blocks tagged `#Template`, inserted with `/Template`. "Apply template to tags" fills in a new node of that tag, such as every new journal.
- Flashcards: blocks tagged `#Card` (or `{{cloze text \\ cue}}`), reviewed on a schedule with FSRS.
- PDF reader with highlights and area captures; each highlight is a block that can be referenced, and annotations list under the PDF.
- YouTube embeds with clickable timestamps `{{youtube-timestamp 90}}`.
- A `/Calculator` block with variables and functions.
- Slash commands for nearly everything; the older `<` commands were merged into `/`.
- Find in page (Ctrl+F) that covers query results and references.
- Page and block icons; favorites and recent in the left sidebar, which can be configured.
- Git auto-commit in the file version, at an interval, without push.
- Hourly automatic backups of DB graphs, last 12 kept.
- Graph view (rebuilt in 2026 for large graphs).
- Publish a page to logseq.io, optionally password protected, or export a static site.
- Export as standard Markdown (without block properties), EDN, or SQLite.
- Plugins in JavaScript, with a marketplace; custom.css and custom.js; themes.
- Local HTTP API, a `logseq` CLI, and an MCP server (DB version).

## 3. What Jot already has

| Logseq                                    | Jot                                                     |
| ----------------------------------------- | ------------------------------------------------------- |
| Linked and unlinked references            | Backlinks with unlinked mentions                        |
| Daily journal, journal template           | Daily notes and templates (#52)                         |
| Quick add to today's journal              | Quick capture to the inbox (#85, #431)                  |
| Natural-language dates in the date picker | Quick add reads `tomorrow 3pm`, `friday`, `nov 14`      |
| Scheduled and deadline, repeats           | `@due`, `@every`, `@daily`, ticking writes the next one |
| Priorities `[#A]`                         | `@p1` to `@p4` (#333)                                   |
| Today's tasks on the journal              | Tasks view for today, upcoming, anytime                 |
| Aliases                                   | `aliases` in front matter, used by links and mentions   |
| Namespaces                                | Notebooks are folders                                   |
| Git auto-commit                           | Git sync with commits and pushes (#227, #371)           |
| Find in page                              | Find and replace (#362)                                 |
| Slash commands                            | Not yet; see the SilverBullet pass                      |
| Page icons                                | Notebook emoji (#120)                                   |
| Mermaid, math                             | Built in (#56, #57)                                     |

## 4. Context for existing issues

- **#59 Note types.** Logseq's supertags are the closest thing to #59 in a shipping app, and they suggest three things Jot's plan does not have yet. A type can extend other types and inherit their properties, so Character and Location can share a Place or Entity base. A property can be limited to notes of one type, so a Scene's `pov` only offers Characters. And a type can have a template applied when a note gets that type, which is #59's "body template" made automatic. Uniqueness by type (`Apple #Company`, `Apple #Fruit`) does not fit Jot, where a note is a file and its name is its path.
- **#60 Properties panel.** Properties have a UI position (shown before the text, after it, or in a row below), and can be hidden by default or when empty. Choices carry icons and one can be the default. A keyboard path matters: Logseq has a shortcut to add a property and letter hints to jump to one.
- **#61 Saved views.** Logseq's DB views are a good checklist: table, list grouped by note, and gallery with a cover image; filters by type with match all or any; group by; multi-column sort; pinned and resized columns; editable cells; several saved views on one type; "+ New" makes a note of that type. Tables keyed by tag mean "all characters" is a page you already have.
- **#72 Relationships.** Bidirectional properties are exactly the reverse side of a relationship: set `author: Ada` on a book, and Ada's note shows her books, without writing anything to Ada's file. Jot can do the same from the index.
- **#70 Codex mentions, #71 Mentions panel.** Logseq's linked references filter is a cheap, proven part of this: on a character's note, filter the references to those that also mention a place or a tag, or hide those that do.
- **#280 Split panes.** Logseq's right sidebar is a stack rather than a second pane: Shift-click opens a note or a block in it, several stack up, each can be collapsed or closed, and they are editable. For reference while writing, that may be simpler than true split panes.
- **#84 Canvas, #449 Drawing.** Logseq dropped whiteboards and drawing from the DB version and hopes plugins pick them up. Both are large to keep up; a separate file type, as #449 plans, is easier to drop or replace than a feature in the core.
- **#88 Web clipper, #90 Capture by email.** Logseq's capture is a `logseq://` URL that a bookmarklet calls with the page URL, title and selection. A `jot://` URL scheme, registered by the desktop entry, would give a web clipper with no browser extension, and let other apps open a note.
- **#350 Search filters.** Logseq's query language is Datalog, which almost nobody writes by hand; the query builder is how most queries get made, and the DB version broke many advanced queries. It supports #350's plan of one simple filter syntax with a builder rather than a second language.
- **#67 Named snapshots.** Logseq keeps a status history on each task, which gives "time in Doing". Jot's note history could answer "when was this ticked" the same way, from history it already keeps.
- **#121 Locked notebook, #81 End-to-end encryption.** Logseq Sync encrypts with a password set once per user under Settings, Encryption, before the first synced graph. It is a reasonable model for #81.
- **#69 Import from Scrivener and novelWriter.** Add Logseq to the list of imports. A file graph is already Markdown; importing means turning `key:: value` lines into front matter, `TODO`/`DONE`/`SCHEDULED`/`DEADLINE` into checkboxes with `@due`, `((uuid))` references into text or links, and journals into daily notes. People leaving over the DB move are the likely users.
- **#415, #514 Web video.** Logseq makes `{{youtube-timestamp 90}}` jump the embedded video. Jot could make a `?t=90` or `1:30` timestamp next to a video seek it.

## 5. New ideas for Jot

Roughly in order of value for the effort. Some overlap the SilverBullet pass; those are noted.

1. **Paste HTML as Markdown, and a raw paste.** Pasting from a web page or a document gives Markdown with headings, lists, links and emphasis; Ctrl+Shift+V pastes plain text. A URL pasted over selected text makes a link. Jot pastes plain text today.
2. **Filter backlinks.** On the backlinks panel, show the tags and notes that the linking paragraphs also mention, and filter to or away from them. Remember the filter per note.
3. **Move a section or selection to another note.** A command that cuts the section at the cursor (or the selection) and appends it to a chosen note, leaving a link behind if wanted. Common when a daily note grows a topic of its own.
4. **Focus on a section.** Show only one heading's section (or one list item and its children) as if it were the whole note, with a breadcrumb back. Jot already folds sections (#522), so this is a step further.
5. **Dates as links to daily notes.** Typing `[[` or a date shortcut offers "Today", "Next Friday" and so on, and writes a link to that day's daily note. A daily note's backlinks then show everything planned for that day.
6. **A continuous journal view.** Daily notes as one scrolling page, newest first, each editable in place. A different take on the SilverBullet pass's previous and next day.
7. **A `jot://` URL scheme.** `jot://open?vault=...&note=...` and `jot://capture?title=&url=&text=`, registered in the desktop entry. Gives #88 a bookmarklet clipper and lets other apps link into Jot.
8. **Clickable video timestamps.** `1:30` or `?t=90` next to an embedded video seeks it.
9. **Logseq import.** As in #69 above: properties, task markers, block references and journals.
10. **Canceled to-dos.** Logseq's statuses include Canceled. A `@cancelled` marker (or `[-]`, which some Markdown tools read) would keep a dropped to-do out of the lists without pretending it was done.

## 6. Not for Jot

- **Blocks as the unit, block ids, `((uuid))` references.** They need an id written into the text, which is what made Logseq's files messy and drove the DB move. Jot's links go to notes and headings.
- **A database as the source of truth.** The clearest lesson in this pass: it split Logseq's users and left Markdown as a read-only mirror. Jot's promise that notes are files and the index is a cache is a selling point against it.
- **Datalog queries.** Too hard for most users, and fragile across versions.
- **Flashcards.** Real for students, but a separate app's job; Anki exists.
- **PDF highlights.** Large, and for researchers more than Jot's writers and worldbuilders. A PDF viewer for attachments is enough for now.
- **Real-time collaboration and paid Sync or Publish.** Ruled out by no Jot account or server.
- **MCP server.** Ruled out by "No AI".
- **Calculator blocks.** Small, but off to the side of notes and writing.

## Sources

- https://github.com/logseq/docs, `db-version.md`, `db-version-changes.md` and `pages/` (2026-10-21)
- https://github.com/logseq/logseq/releases (2.0.2, 2026-10-07)
- https://discuss.logseq.com/t/logseq-db-changelog/30013/37 (Markdown Mirror, May 2026)
- https://discuss.logseq.com/t/is-anyone-planning-to-use-og-version-indefinitely/35091
- https://chromic.org/blog/logseq-db-git-sync/
