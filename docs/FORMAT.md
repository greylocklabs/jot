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
    layout.yaml              notebook emoji and pinned notes
    history/                 past versions of notes
    joplin.yaml              what imports from Joplin brought, once there has been one
    trash/                   what was deleted, where the drive has no trash
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
- A note is a `.md` file. Other files are attachments, shown but not edited as notes.
- Names starting with `.` are hidden from the app and the index, apart from `.jot/`.
- Opening a folder in Jot makes `.jot/vault.yaml` with `format: 1`, if it has none.
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

| Key       | Type                       | Meaning                                        |
| --------- | -------------------------- | ---------------------------------------------- |
| `title`   | string                     | Title, when the file name cannot hold it       |
| `tags`    | list of strings            | Tags, nested with `/`: `cast/lead`             |
| `aliases` | list of strings            | Other names links and mentions can use         |
| `created` | ISO 8601 date-time         | When the note was created                      |
| `task`    | `open` or `done`           | Makes the note a task                          |
| `due`     | ISO 8601 date or date-time | When a task is due                             |
| `done`    | ISO 8601 date-time         | When a task was completed                      |
| `labels`  | list of strings            | A task's labels                                |
| `type`    | string                     | The note's type; searchable as `type:` for now |

Rules:

- Every other key is kept exactly as written.
- When Jot changes a key, it rewrites only that key's lines, so comments, order and quoting in the
  rest of the front matter survive.
- Jot writes `created` once, when you make a note or import one, because file times do not survive
  copies and sync. A note without it shows the file's modification time.
- Jot writes `created` and `done` in UTC, ending in `Z`. A `due` with a time and no offset is in
  local time.
- Jot never writes an `updated` key. The file's modification time is the update time, which keeps
  saves from rewriting the front matter.
- No id is required or written. Links work by path and name.
- A list key also accepts a single string, which is one item: `tags: a, b` is the one tag `a, b`.
  `aliases` also accepts `alias`, `tags` `tag`, and `labels` `label`. A leading `#` on a tag and
  `@` on a label are dropped, and `task:` ignores case.
- Reopening a task removes its `done:`. Making a note no longer a task removes `task:` and leaves
  `due:`.

### Body

CommonMark with the GitHub extensions (tables, task lists, strikethrough, autolinks), plus:

- `[[wikilinks]]`, `[[Note|label]]`, `[[Note#Heading]]` and `![[embeds]]`
- inline tags: `#tag` and `#tag/nested`, outside code and links. A tag is letters, digits, `_`,
  `-` and `/`, with at least one that is not a digit, and starts the line or follows a space, `(`
  or `,`. Tags match without regard to case.

Jot keeps any other Markdown as written, footnotes and math among them, and does not draw those
yet.

## Links

Jot reads standard Markdown links and wikilinks, to notes and attachments alike. It writes:

- a wikilink when it links to a note, as `[[` completion and Copy link do: `[[Soren]]`, or
  `[[Ashfall/Soren]]` when another note has the same name;
- a Markdown link, relative to the note, when it adds an attachment: `![](Mara.assets/sketch.png)`.
  Spaces become `%20`, and a path with `(` or `)` is written `<like this>`.

Jot resolves a link in this order:

1. a path from the linking note's folder, then from the vault root, with or without `.md`;
2. a note by name, ignoring case, where a name with folders must match the end of the path:
   one in the linking note's folder first, then the one with the shortest path;
3. a note by alias;
4. for a bare file name such as `![[map.png]]`, an attachment in the linking note's `.assets`
   folder, then beside the note.

When a note is renamed or moved in Jot, every link to it, and every link into its `.assets`
folder, is rewritten. A Markdown link keeps its style, with or without `.md`. A link whose target
is missing stays as written.

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

Where the drive has no trash, a deleted note, attachments folder or notebook goes to
`.jot/trash/<yyyyMMdd-HHmmss>/` instead, with a number in front of a name already there:
`2 Mara.md`.

## Tasks

There are two kinds, and the task view shows both:

- **Checkboxes** in any note: a list item starting `[ ]` or `[x]` (or `[X]`), after `-`, `*`, `+`,
  `1.` or `1)`. Ticking one in Jot edits that line.
- **Task notes**: a note with `task: open` or `task: done` in its front matter, and optionally
  `due`, `done` and `labels`. Completing one sets `task: done` and `done:` to the current time.

A checkbox says more about itself on its own line, in the format of Obsidian's Tasks plugin, so
the same vault works there:

```markdown
- [ ] Call Soren @minutes #calls 📅 2026-10-29 14:00
- [ ] Water the plants @day 🔁 every week 📅 2026-10-30
- [x] Pay rent ✅ 2026-10-01
```

- `📅` and a date is when it is due. A time after the date, `14:00`, `T14:00` or `9:30`, is Jot's
  addition, in local time; Obsidian shows it as text.
- `🔁` and a rule is how it repeats, `✅` and a date when it was done. Jot reads `every day`,
  `every week`, `every month` and `every year`, with a number (`every 2 weeks`) or `other`,
  `every weekday`, `every monday` through `every sunday`, and `every 2 weeks on monday`, in any
  case. Ticking one that is due writes the next one above it, unticked and due on its next day
  after today, and gives the ticked one `✅` and today's date, as Obsidian does. A rule Jot does
  not read is kept, and only ticked.
- A checkbox that does not repeat is only ticked or unticked; a `✅` already there stays.
- `@label` gives it a label, like Todoist's: `@` after a space or at the start, then letters,
  digits, `_` and `-`. `#tags` work as they do anywhere in a note.
- Each marker's value runs to the next marker, and a marker may carry the emoji variation
  selector (U+FE0F). Obsidian's other markers (`⏳`, `🛫`, `➕`, `❌` and the priorities) are kept
  as written, and not yet read.

Quick add appends a checkbox to the note `inbox:` names in `.jot/vault.yaml`, `Inbox.md` at the top
of the vault by default, making the note if need be. It writes the repeat before the date:

```markdown
- [ ] Call Soren @minutes 🔁 every week 📅 2026-10-29 14:00
```

## Notebook appearance and pins

How the note list is ordered (by when notes changed, when they were made, or by title) is each
computer's own setting, not part of the vault. The notebook tree sorts in natural number order
(`Ch 2` before `Ch 10`).

`.jot/layout.yaml` keeps what belongs to the vault, so it never changes a note file:

```yaml
folders:
  Ashfall:
    emoji: 📚
pinned:
  - Recipes/Pancakes.md
```

- Paths are relative to the vault root and use `/`.
- `emoji` is shown before the notebook's name.
- `pinned` notes stay at the top of the note list, in whatever order it is in.
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
- Edits made outside Jot keep no versions, and nothing is pruned yet.

## Vault settings

`.jot/vault.yaml` holds settings that belong to the vault rather than to one computer:

```yaml
format: 1
inbox: Inbox.md # where quick add puts to-dos, as a checkbox at the end
```

- `format` is the vault's format number, as [Versioning](#versioning) says. One that is missing
  or not a whole number is read as 1.
- `inbox` is a note's path from the vault root, with or without `.md`. One starting with `/` or
  `.`, or containing `../`, is ignored, and `Inbox.md` used instead.

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
| To-do, `todo_completed`, `todo_due` | `task:`, `done:`, `due:`                                     |
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
