# Vault format

A Jot vault is a folder of Markdown files that any editor can read and change. This document is
what Jot reads and writes. Where it says "Jot writes", Jot also reads the alternatives listed, so
vaults from Obsidian, Joplin exports and plain Markdown folders open unchanged.

Status: final, format 1, as of Jot 0.1. Changes follow [Versioning](#versioning).

## Layout

```text
My Notes/                    the vault: any folder
  .jot/                      Jot's files for the vault, synced with the notes
    vault.yaml               vault settings and the format number
    layout.yaml              notebook emoji, sorts, word targets, favorite and hidden notes
    history/                 past versions of notes
    joplin.yaml              what imports from Joplin brought, once there has been one
    trash/                   what was deleted, in Jot and by sync
    stats/                   words written each day, a file for each computer
  Inbox.md                   where quick add puts to-dos
  Recipes/
    Pancakes.md
  Ashfall/
    Characters/
      Mara.md
      Mara.assets/           Mara's attachments
        sketch.png
      Soren.md
```

- Folders are notebooks, nested to any depth. A link to a folder is shown but not followed.
- A note is a `.md` file. Data files, `.json`, `.csv`, `.tsv`, `.yaml` and `.yml`, are listed
  beside the notes under their full file names and edited as they are, with nothing added to
  them. Other files are attachments, shown but not edited.
- A data file that is one list of records opens as a table. One holding several, as lists under
  the keys of its top-level object or mapping, or YAML documents that are each a list, opens with a
  tab for each; only the keys of a single YAML document make tabs.
- A data file is checked against a JSON Schema (draft 2020-12) when one is beside it: the file its
  top-level `$schema` names, else `<name>.schema.json`. A schema on the web is never fetched. For
  a list under a key, the schema's `properties` for that key describes it; YAML documents are not
  checked.
- Names starting with `.` are hidden from the app and the index, apart from `.jot/`. So is what
  the vault ignores, as [Vault settings](#vault-settings) says.
- Opening a folder in Jot makes `.jot/vault.yaml` with `format: 1`, if it has none.
- `.jot/stats/<computer>.yaml` holds `YYYY-MM-DD: words` lines: the words that computer's edits
  added to the vault's notes each day, less those they took away. Each computer writes only its
  own file, named for the computer and an id it keeps in its own settings, and Jot adds them up.
- Jot's own machine-local state (search index, link index, caches) lives under
  `$XDG_STATE_HOME/jot` and `$XDG_CACHE_HOME/jot`, never in the vault. Deleting it loses nothing:
  Jot rebuilds it from the files.

## Notes

### Encoding and bytes

- UTF-8, read and written explicitly. A byte order mark is kept if the file had one. A file that
  is not UTF-8 is not opened, so it is never written back with replacement characters.
- Each line keeps the line ending it had. A line changed in place keeps the ending it had, and an
  added line takes the ending most of the file's lines have. A new note uses `\n`.
- Saving a note writes back exactly the bytes in the editor. Jot never reformats a note's
  Markdown or YAML on load or save.
- Writes are atomic: a temporary file in the same folder, then a rename. A file with more than one
  hard link is written in place instead, and a symbolic link is written through to its target,
  so the links survive.
- When a note changes on disk while it has edits not yet saved, Jot merges the two line by line.
  Where both changed the same lines, or lines next to each other, the other version is kept
  beside the note as `<file name> (conflict <yyyy-MM-dd HHmm>).md`, and Jot's version is saved.

### Titles and file names

The file name, without `.md`, is the title. Renaming a note in Jot renames the file and updates
every link to it.

A title becomes a file name as follows: `/` and `\` become a `-` with a space each side;
`:` `*` `?` `"` `<` `>` `|` and control characters are dropped; runs of spaces become one; and
leading and trailing dots and spaces are trimmed. A title left empty is "Untitled", in the app's
language. When the file name cannot hold the title, the real title is kept in front matter:

```markdown
---
title: "Q&A / Mara"
---
```

A `title:` key always wins over the file name. Jot writes it only when the two differ.

A second note with the same name in the same folder gets a number: `Pancakes 2.md`.

### Front matter

Optional YAML between a `---` line at the very start of the file and the next `---` or `...`
line. Anything else, including an unclosed fence, is body text.

Jot reads a simple subset of YAML: `key: value` with plain or quoted strings, and lists written
`[a, b]` or as `- item` lines. Anything else, such as nested maps, is kept as text.

Keys Jot understands:

| Key       | Type                      | Meaning                                                |
| --------- | ------------------------- | ------------------------------------------------------ |
| `title`   | string                    | Title, when the file name cannot hold it               |
| `tags`    | list of strings           | Tags, nested with `/`: `cast/lead`                     |
| `aliases` | list of strings           | Other names links and mentions can use                 |
| `created` | ISO 8601 date-time        | When the note was created                              |
| `type`    | string                    | The note's type; searchable as `type:` for now         |
| `target`  | whole number              | Words the note is meant to reach                       |
| `lang`    | language code, as `en_GB` | The language spelling is checked in                    |
| `tasks`   | `off`                     | Keeps the note's checkboxes out of the lists of to-dos |

Rules:

- Every other key is kept exactly as written.
- Jot warns, without changing anything, about a fence never closed, a line that is no
  `key: value` or list item, and a value of the wrong kind for a key above.
- When Jot changes a key, it rewrites only that key's lines, so comments, order and quoting in the
  rest of the front matter survive.
- Jot writes `created` once, when you make a note or import one, because file times do not survive
  copies and sync. A note without it shows the file's modification time.
- Jot writes `created` in UTC, ending in `Z`.
- Jot never writes an `updated` key. The file's modification time is the update time, which keeps
  saves from rewriting the front matter.
- No id is required or written. Links work by path and name.
- A list key also accepts a single string, which is one item: `tags: a, b` is the one tag `a, b`.
  `aliases` also accepts `alias`, and `tags` `tag`. A leading `#` on a tag is dropped.
- A note is never a to-do itself: `task:`, `due:`, `done:`, `repeat:` and `priority:`, which Jot
  0.5 read as a task note, are ordinary keys, kept as written.

### Body

CommonMark with the GitHub extensions (tables, task lists, strikethrough, autolinks), plus:

- inline tags: `#tag` and `#tag/nested`, outside code and links. A tag is letters, digits, `_`,
  `-` and `/`, with at least one that is not a digit, and starts the line or follows a space, `(`
  or `,`. Tags match without regard to case.
- `${toc}` on a line of its own, as Joplin writes it, for a list of the note's `#` headings,
  drawn in its place and never written into the file.
- LaTeX math: `$...$` within a line, as Pandoc reads it, and display math between `$$` lines or
  as `$$...$$` on one.
- CriticMarkup on one line: `{++added++}`, `{--deleted--}`, `{~~old~>new~~}`,
  `{==highlighted==}` and `{>>comments<<}`, shown as edits, and comments as notes in the margin.

Jot keeps any other Markdown as written, footnotes among them, and does not draw those yet.
Wikilinks, such as `[[Note]]`, are words to Jot, as they are to other Markdown tools.

## Links

Links are standard Markdown links, to notes and attachments alike, so other editors follow them
too. Jot writes:

- a path from the note's folder, as completion after `](` and the Link shortcut do, and when it
  adds an attachment: `[Soren](Characters/Soren.md)`, `![](Mara.assets/sketch.png)`;
- a path from the vault's root, starting with `/`, for Copy link, so the link works pasted into a
  note in any folder: `[Soren](/Ashfall/Characters/Soren.md)`.

Spaces become `%20`, and a path with `(` or `)` is written `<like this>`. Following a link to a
`.md` file that is not there yet makes that note.

Jot also reads reference links, `[Soren][1]` with `[1]: Characters/Soren.md` on a line of its own,
and resolves and rewrites the definition's target like any other link. It never writes them.

Jot resolves a link in this order:

1. a path from the linking note's folder, then from the vault root, with or without `.md`; one
   starting with `/` only from the vault root;
2. a note by name, ignoring case, where a name with folders must match the end of the path:
   one in the linking note's folder first, then the one with the shortest path;
3. a note by alias;
4. for a bare file name such as `![](map.png)`, an attachment in the linking note's `.assets`
   folder, then beside the note.

When a note is renamed or moved in Jot, every link to it, and every link into its `.assets`
folder, is rewritten. A link keeps its style: with or without `.md`, and from the note's folder or the
vault's root. A link whose target is missing stays as written.

## Attachments

Jot writes attachments beside the note, in a folder named after it:

```text
Mara.md
Mara.assets/
  sketch.png
  Pasted 2026-10-14 153012.png
```

- The folder is made when the first file is added. A folder left empty, as when its files are
  deleted in a file manager, is removed when Jot opens the vault or the note.
- Renaming or moving a note renames or moves its `.assets` folder with it.
- Moving a note to the trash takes its `.assets` folder too, unless another note links into it.
  Jot then asks, and keeps the folder unless told to trash it as well.
- Pasted images are named `Pasted <yyyy-MM-dd HHmmss>.<ext>`, with "Pasted" in the app's language.
- A name already in the folder gets a number: `sketch 2.png`. Jot never overwrites an attachment.
- Any file can be an attachment. Images are drawn in the reading view; other files are links.
- Jot follows a link to an attachment wherever it is, so attachments kept in other folders, as
  other apps keep them, work too.

Each `*.assets` folder belongs to the note with the same name. Jot hides it in the notebook tree
and shows its files under the note instead. One with no note of that name is a notebook.

A deleted note, attachments folder or notebook goes to the vault's own trash,
`.jot/trash/<yyyyMMdd-HHmmss>/`, with its path kept, as `A/Mara.md`. One path deleted twice in the
same second goes to `<yyyyMMdd-HHmmss> 2/`. Jot 0.3 and earlier kept only the name, with a number
in front of one already there (`2 Mara.md`). Jot lists what is there under Trash, puts it back
where it was, and deletes it for good when asked. With the setting on, what has been there 30 days
goes on to the desktop's trash. The trash is never synced.

## Tasks

A to-do is a checkbox in any note: a list item starting `[ ]` or `[x]` (or `[X]`), after `-`,
`*`, `+`, `1.` or `1)`. Ticking one in Jot edits that line.

A checkbox says more about itself in plain words on its own line, each starting with an `@`:

```markdown
- [ ] Call Soren #calls @due 2026-10-29 14:00 @p1
- [ ] Water the plants @weekly @due 2026-10-30
- [ ] Review the budget @every 2 weeks on monday @due 2026-11-02
- [x] Pay rent @monthly @due 2026-10-01 @done 2026-10-01
```

- `@due` and a date is when it is due. A time after the date, `14:00`, `T14:00` or `9:30`, is in
  local time.
- `@daily`, `@weekly`, `@monthly`, `@yearly` and `@weekdays` say how it repeats, and `@every` with
  a rule for anything else: Jot reads `every day`, `every week`, `every month` and `every year`,
  with a number (`every 2 weeks`) or `other`, `every weekday`, `every monday` through
  `every sunday`, and `every 2 weeks on monday`, in any case. A rule is the most of the words
  after `@every` that make one of these, so words after it, even a number, stay the to-do's
  own.
- `@done` and a date is when it was done. Ticking one that repeats and is due writes the next one
  above it, unticked and due on its next day after today, and gives the ticked one `@done` and
  today's date. A rule Jot does not read is kept, and only ticked.
- `@done` alone, with no date, is done on a day not said; a checkbox with any `@done` is done,
  ticked or not. In Jot's editor, `@done` alone, and a day said another way after `@due` or
  `@done`, as quick add reads it (`tomorrow 3pm`, `yesterday`, `friday`, `nov 14`, or
  `11-14-2026` with the month first), are written as dates when the cursor leaves the line, and a
  to-do said done is ticked.
- `@p1` to `@p4` is its priority: urgent, high, medium or low. One without any has none.
- A checkbox that does not repeat is only ticked or unticked; a `@done` already there stays.
- Markers are found case aside, each at the start of a word, so `hello@example.org` is no marker.
  `@due` and `@done` without a valid date, and any other `@word`, are just words.
- `#tags` work as they do anywhere in a note.

Quick add appends a checkbox to the note `inbox:` names in `.jot/vault.yaml`, `Inbox.md` at the top
of the vault by default, making the note if need be. It reads `p1` to `p4` as the priority, as
Todoist does, and writes the repeat, the date and the priority in that order:

```markdown
- [ ] Call Soren #calls @weekly @due 2026-10-29 14:00 @p1
```

## Notebook appearance and favorites

How the note list is ordered by default (by when notes changed, when they were made, or by
title) is each computer's own setting; a notebook can keep a sort of its own in the vault. The
notebook tree sorts in natural number order (`Ch 2` before `Ch 10`).

`.jot/layout.yaml` keeps what belongs to the vault, so it never changes a note file:

```yaml
folders:
  Ashfall:
    emoji: 📚
    sort: title
    target: 90000
    deadline: 2027-01-30
favorites:
  emoji: ⭐
  notes:
    - Recipes/Pancakes.md
hidden:
  - Archive
trash:
  emoji: ""
```

- Paths are relative to the vault root and use `/`.
- `emoji` is shown before the notebook's name.
- `sort` is how the notebook's notes are listed: `updated`, `created` or `title`. Without it,
  the computer's default applies. Notebooks inside it have their own.
- `target` is the words the notebook's notes, and those of the notebooks inside it, are to come
  to, and `deadline` the day, YYYY-MM-DD, to reach it by. A deadline without a target means
  nothing.
- `favorites` lists the favorite notes under `notes`, in the order Favorites in the sidebar
  shows them, and its `emoji`, ⭐ when unset. It replaced the `pinned` list of earlier versions,
  which Jot no longer reads.
- `hidden` notes and notebooks, and everything in a hidden notebook, stay out of the note list,
  quick open, search, Tasks and tags, in Jot and in `jot`. Links to them still work. Unlike
  ignored files, Jot still reads them.
- `tasks` and `trash` hold the `emoji` of Tasks and Trash in the sidebar, shown as ✅ and 🗑️ when
  unset; `""` shows none, for them and for `favorites`.
- Renaming or moving a note or notebook in Jot takes its entries with it.
- Entries that no longer exist are ignored and pruned on the next write.
- Jot keeps what it does not use as written: other sections, and values nested under a
  notebook. Comments inside the sections it writes are not kept. A file Jot cannot read is left
  alone, and changes to it are refused until it is fixed.

## History

Jot keeps past versions of each note as plain copies under `.jot/history/`, in a folder named
for the note's path, and names each copy by when the note held it:

```text
.jot/history/Ashfall/Characters/Mara.md/
  2026-10-12 091500.md
  2026-10-28 153012.md
  2026-10-28 153012 2.md     a second version from the same second
```

- Times are local, to the second.
- Before Jot first writes a note it has open, it keeps the version being replaced, then at most
  one every ten minutes while editing goes on. Ticking a to-do from a list keeps one the same way.
- A version the same as the newest one, or an empty one, is not kept.
- Restoring a version keeps the text it replaces as a version too, so a restore can be undone.
- Renaming or moving a note or notebook in Jot moves its history with it. A deleted note's
  history stays, and a note later made or moved to its path takes it on.
- Edits made outside Jot keep no versions.
- Versions thin out as they age, whenever Jot keeps a new one: every version from the last day
  stays, then the newest in each hour for a week, in each day for a month, and in each week
  after that. The newest version always stays.

## Sync

A vault can sync with a remote, such as another folder. The remote holds the vault's files as
they are, so it can be read without Jot.

- Everything in the vault syncs but hidden files and folders, which stay on each computer,
  `.jot/trash/` and what the vault ignores. Jot's own `.jot/` folder does sync, history included.
  An ignored file is left alone on both sides, so ignoring one that synced before deletes it
  nowhere, and git sync keeps the vault's `ignore` patterns out of its commits too.
- Where to sync is a setting of each computer, not of the vault. What Jot knows of the last sync,
  to tell an edit from a delete, is kept under `$XDG_STATE_HOME/jot/sync/`, never in the vault.
  Deleting it loses nothing: the next sync compares the two sides afresh, keeping both versions
  of any file that differs.
- A file changed on both sides since the last sync is merged line by line, its front matter key
  by key. Where the two changed the same thing differently, the version from the other side is
  kept beside the file as `<file name> (conflict <computer> <yyyy-MM-dd HHmm>).<extension>`, on
  both sides.
- A file deleted on one side and changed on the other is kept, changed.
- A git remote is a repository the vault is committed to, a commit for each sync. Git's own
  files live in Jot's state folder, not the vault, and Jot writes `.gitattributes` to keep
  pictures, PDFs, audio, video and archives in Git LFS. A vault that already is a repository is
  synced as it is set up: on its own origin and branch, with its own `.gitattributes`. Its origin
  counts as the remote however either is written, SSH or HTTPS, with or without `.git`. Where
  git cannot merge a note, Jot merges it as for the other remotes, or keeps the other version as
  a conflict copy; it never leaves conflict markers in a file.
- A Dropbox remote holds the files in Jot's own app folder, `Apps/Jot`, by their paths. Dropbox
  keeps what a sync deletes for a while, as its own trash.
- A Google Drive remote holds the files in a folder for the vault, named as the vault is unless
  given another name, in a folder called `Jot` at the top of the Drive, in folders by their
  paths. Jot asks to see only what it made, so files put there in Drive's own
  app stay out of the vault. What a sync deletes goes to Drive's trash. Drive cannot refuse a
  write over a version other than the one expected, so Jot checks the version just before each
  write; two computers writing the same file at that same moment could still overwrite each
  other.
- An S3 remote holds each file as an object named by its path, under the folder given in the
  bucket. The store must support conditional writes (`If-Match` and `If-None-Match`), which Jot
  checks before syncing with it, so two computers never overwrite each other unseen. The access
  key is kept in the desktop's keyring, not in Jot's settings.
- What a sync deletes goes to `.jot/trash/<yyyy-MM-dd HHmmss>/` on that side, with its path
  kept, so it can be brought back. A folder remote has its own `.jot/trash/` for the same.
- The remote holds a `.jot-remote` file at its top, an id written by the first sync, which no
  vault ever gets. A remote without it, or with another id than the one synced with before, is
  taken for an unplugged drive, an unmounted share or another remote: nothing is changed, and
  the sync waits. Stopping sync on a computer forgets what it knew, so the next sync starts
  afresh.

## Vault settings

`.jot/vault.yaml` holds settings that belong to the vault rather than to one computer:

```yaml
format: 1
inbox: Inbox.md # where quick add puts to-dos, as a checkbox at the end
templates: Templates # the notebook whose notes are templates
daily: Daily # where daily notes go, one per day as YYYY-MM-DD.md
language: en_GB # spelling is checked in this, unless a note says otherwise
ignore: [node_modules/, "*.log"] # files and folders Jot leaves out
gitignore: false # leave .gitignore files out of it; they count unless this says false
gitMessage: "chore(sync): jot commit from {device}, {date} {time}" # git sync's commit message
gitHooks: false # whether git sync's commits run the repository's hooks
gitSign: true # sign git sync's commits; unset leaves it to git
gitPull: merge # bring in what others pushed by merging; rebase when missing
gitCommit: manual # commit only when asked; each sync commits when missing
gitPush: manual # push only when asked; each sync pushes when missing
syncEvery: 15 # minutes between automatic syncs; 0 syncs only when asked
```

- `format` is the vault's format number, as [Versioning](#versioning) says. One that is missing
  or not a whole number is read as 1.
- `inbox` is a note's path from the vault root, with or without `.md`. One starting with `/` or
  `.`, or containing `../`, is ignored, and `Inbox.md` used instead.
- `templates` and `daily` are notebooks' paths from the vault root, `Templates` and `Daily` when
  missing, and ignored when they start with `/` or any part of them
  starts with `.`. A new note from a template takes the
  template's front matter keys it does not have and its text, with `{{title}}`, `{{date}}`,
  `{{time}}`, `{{date:FORMAT}}` and `{{time:FORMAT}}` filled in, FORMAT as Obsidian writes it.
  A daily note is made from `Daily.md` in the templates notebook, when there is one.
- `ignore` is one pattern or a list of them, read as the lines of a `.gitignore` at the top of the
  vault: `Drafts/` for a folder at any depth, `/Drafts/` for the one at the top, `*.log` for files
  by name, `**` across folders, and `!` to bring back what an earlier pattern left out. What it
  matches stays out of the app, the index, search, tasks and sync. Unless `gitignore` is `false`,
  the vault's `.gitignore` files count too, as git reads them, deeper ones after the vault's own
  patterns. Nothing inside an ignored folder comes back, and `.jot/` is never ignored.
- `gitMessage`, `gitHooks` and `gitSign` say how git sync commits, the same on every computer.
  The message fills in `{device}`, `{date}`, `{time}`, `{count}` (files changed) and `{files}`
  (up to three of them); it is `chore(sync): jot commit from {device}, {date} {time}` when
  missing. Hooks run only when `gitHooks` is `true`. Commits are signed with `gitSign: true`,
  not with `false`, and as git's own settings say when it is missing. They are made as git's
  user, or as `Jot <jot@localhost>` when git has none.
- `gitPull` is how git sync brings in what others pushed: `rebase`, the default, keeps the
  history in a line, and `merge` merges it. A rebase that stops on a conflict is given up for a
  merge, so notes are still merged as for the other remotes, or kept as conflict copies.
- `gitCommit: manual` leaves what changed uncommitted until Commit is asked for, with a message
  filled in from `gitMessage`; syncs still bring in what others pushed, but not into a file
  changed here and not committed yet. `gitPush: manual` keeps commits here until Push is asked
  for. Both are `auto` when missing.
- `syncEvery` is the minutes between automatic syncs, for every kind of remote: 5 when missing.
  With 0, Jot syncs only when asked, with F5 or Sync now, and not when the vault opens or after
  an edit either.

## Versioning

- Adding a file, a key or a value that an older Jot can ignore is not a change of format. Every
  Jot keeps what it does not know as written, and ignores it.
- `format` goes up only when an older Jot would misread or damage something in a vault that uses
  the change. Each such change says how a vault moves from the format before.
- Jot reads a vault whose `format` is newer than it understands, but writes nothing to it, and
  says so.

## Importing from Joplin

Jot reads Joplin's own database, `database.sqlite` in the Joplin app's profile folder, with the
attachments in `resources/` beside it, or a JEX export. The database is only read, and is best
read while Joplin is closed. Either maps onto this format as follows:

| Joplin                              | Jot                                                          |
| ----------------------------------- | ------------------------------------------------------------ |
| Notebook                            | Folder, merged with one of the same name                     |
| Notebook icon                       | Its emoji, in `.jot/layout.yaml`, unless one is set in Jot   |
| Note title                          | File name, with `title:` when it had unsafe characters       |
| Tags                                | `tags:`                                                      |
| `created_time`, `updated_time`      | `created:`, and the file's modification time                 |
| To-do, `todo_completed`, `todo_due` | A checkbox atop the note, with `@due` and `@done`            |
| Resource used by notes              | File in the `.assets` folder of the first note that uses it  |
| `:/id` links                        | Relative Markdown links                                      |
| HTML notes                          | Kept as HTML inside the `.md` file, with its links rewritten |
| `source_url`, `author`              | Kept as front matter keys                                    |
| `latitude`, `longitude`, `altitude` | Left out: Jot does not keep where a note was written         |
| Conflict copies                     | A note called `<title> (conflict)` beside the original       |
| Notes and notebooks in the trash    | Left out                                                     |
| Revisions, from Joplin's database   | Past versions in the note's history, at Joplin's times       |
| Revisions in a JEX export           | Left out: only the database brings them                      |
| Encrypted items                     | Left out, and counted in the import's summary                |

Importing again is safe, so a Joplin account can come across one notebook at a time.
`.jot/joplin.yaml` records which file each Joplin note and notebook became, and what each note
held when imported. A later import of the same or an overlapping export then:

- leaves alone notes that Joplin has not changed;
- brings up to date notes changed in Joplin but not in Jot;
- leaves as they are notes edited in Jot since, and says so;
- does not bring back notes removed in Jot, and finds ones moved without edits;
- moves a notebook imported without its parent under that parent, once the parent is imported.

Deleting `.jot/joplin.yaml` only makes the next import treat everything as new.
