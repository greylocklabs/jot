# Vault format

A Jot vault is a folder of Markdown files that any editor can read and change. This document is
what Jot reads and writes. Where it says "Jot writes", Jot also reads the alternatives listed, so
vaults from Obsidian, Joplin exports and plain Markdown folders open unchanged.

Status: draft, version 1. Nothing here is final until Jot 0.1 ships.

## Layout

```text
My Notes/                    the vault: any folder
  .jot/                      vault settings, synced with the notes
    vault.yaml
    layout.yaml              manual order, notebook emoji, icons, colors and pins
    types/                   note types (later)
    views/                   saved views (later)
    templates/
  Inbox/
    Pancakes.md
  Ashfall/
    Characters/
      Mara.md
      Mara.assets/           Mara's attachments
        sketch.png
      Soren.md
```

- Folders are notebooks, nested to any depth.
- A note is a `.md` file. Other files are attachments, shown but not edited as notes.
- Names starting with `.` are hidden from the app and the index, apart from `.jot/`.
- Jot's own machine-local state (search index, link index, sync state, caches) lives under
  `$XDG_STATE_HOME/jot` and `$XDG_CACHE_HOME/jot`, never in the vault. Deleting it loses nothing:
  Jot rebuilds it from the files.

## Notes

### Encoding and bytes

- UTF-8, read and written explicitly. A byte order mark is kept if the file had one.
- Line endings are kept as found. New notes use `\n`.
- Saving a note writes back exactly the bytes in the editor. Jot never reformats a note's
  Markdown or YAML on load or save.
- Writes are atomic: a temporary file in the same folder, then a rename.
- When a note changes on disk while it has edits not yet saved, Jot merges the two line by line.
  Where both changed the same lines, or lines next to each other, the other version is kept
  beside the note as `<title> (conflict <date> <time>).md`, and Jot's version is saved.

### Titles and file names

The file name, without `.md`, is the title. Renaming a note in Jot renames the file and updates
every link to it.

Characters that are unsafe in file names on any sync target are replaced with `-` for `/` and
`\`, and with nothing for `:` `*` `?` `"` `<` `>` `|` and control characters. Leading and trailing
dots and spaces are trimmed. When the replacement changes the title, the real title is kept in
front matter:

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

Keys Jot understands:

| Key       | Type                       | Meaning                                         |
| --------- | -------------------------- | ----------------------------------------------- |
| `title`   | string                     | Title, when the file name cannot hold it        |
| `tags`    | list of strings            | Tags, nested with `/`: `cast/lead`              |
| `aliases` | list of strings            | Other names links and mentions can use          |
| `created` | ISO 8601 date-time         | When the note was created                       |
| `task`    | `open` or `done`           | Makes the note a task                           |
| `due`     | ISO 8601 date or date-time | When a task is due                              |
| `done`    | ISO 8601 date-time         | When a task was completed                       |
| `labels`  | list of strings            | A task's labels, with or without `@`            |
| `type`    | string                     | Note type, for templates and properties (later) |

Rules:

- Every other key is kept exactly as written, and shown as a property.
- When Jot changes a key, it rewrites only that key's lines, so comments, order and quoting in the
  rest of the front matter survive.
- Jot writes `created` once, when it creates or imports a note, because file times do not survive
  copies and sync. A note without it shows the file's modification time.
- Jot never writes an `updated` key. The file's modification time is the update time, which keeps
  saves from rewriting the front matter.
- No id is required or written. Links work by path and name.
- `tags` also accepts a single string. `aliases` also accepts `alias`, and `labels` `label`.

### Body

CommonMark with the GitHub extensions (tables, task lists, strikethrough, autolinks), plus:

- `[[wikilinks]]`, `[[Note|label]]`, `[[Note#Heading]]`
- inline tags: `#tag` and `#tag/nested`, outside code and links
- footnotes: `[^1]`
- math: `$inline$` and `$$block$$`
- front matter, as above

## Links

Jot writes standard Markdown links to notes and attachments, with paths relative to the note and
spaces encoded as `%20`:

```markdown
[Soren](Soren.md)
![](Mara.assets/sketch.png)
```

Jot also reads `[[wikilinks]]` and `![[embeds]]`, and resolves them the way Obsidian does: an exact
path from the vault root, then the shortest unique file name, then an alias. The user can choose
to have Jot write wikilinks for note-to-note links.

When a note or attachment is renamed or moved, Jot rewrites every link to it in the vault. A link
whose target is missing is shown as broken and listed, never deleted.

## Attachments

Jot writes attachments beside the note, in a folder named after it:

```text
Mara.md
Mara.assets/
  sketch.png
  Pasted 2026-10-14 153012.png
```

- The folder is created when the first file is added, and removed when it is empty.
- Renaming or moving a note renames or moves its `.assets` folder with it.
- Deleting a note moves its `.assets` folder to the system trash with it, unless another note
  links into it. Jot then asks. On a drive without a trash, both go to `.jot/trash/` in the vault.
- Pasted images are named `Pasted <date> <time>.<ext>` and can be renamed in the app.
- A name already in the folder gets a number: `sketch 2.png`. Jot never overwrites an attachment.
- Any file can be an attachment. Images, audio, video and PDFs are shown inline; other files as a
  link.

A vault setting in `.jot/vault.yaml` picks another layout for new attachments: one `assets/`
folder per notebook folder, or one folder for the whole vault. Jot reads every layout, whatever the
setting.

Each `*.assets` folder belongs to the note with the same name. Jot hides it in the notebook tree
and shows its files under the note instead.

## Tasks

There are two kinds, and the task view shows both:

- **Checkboxes** in any note: `- [ ] task` and `- [x] task`. Ticking one in Jot edits that line.
- **Task notes**: a note with `task: open` or `task: done` in its front matter, and optionally
  `due`, `done` and `labels`. Completing one sets `task: done` and `done:` to the current time.

A checkbox says more about itself on its own line, in the format of Obsidian's Tasks plugin, so
the same vault works there:

```markdown
- [ ] Call Soren @minutes #calls 📅 2026-10-29 14:00
- [ ] Water the plants @day 🔁 every week 📅 2026-10-30
- [x] Pay rent ✅ 2026-10-01
```

- `📅` and a date is when it is due. A time after the date, `14:00`, is Jot's addition; Obsidian
  shows it as text.
- `🔁` and a rule is how it repeats, `✅` and a date when it was done. Jot reads `every day`,
  `every week`, `every month` and `every year`, with a number (`every 2 weeks`) or `other`,
  `every weekday`, and `every monday` through `every sunday`. Ticking one that is due writes the
  next one above it, unticked and due on its next day after today, and gives the ticked one `✅`
  and today's date, as Obsidian does. A rule Jot does not read is kept, and only ticked.
- `@label` gives it a label, like Todoist's. `#tags` work as they do anywhere in a note.
- Each marker's value runs to the next marker. Obsidian's other markers (`⏳`, `🛫`, `➕`, `❌`
  and the priorities) are kept as written, and not yet read.

## Order, pins and notebook appearance

Notes and folders sort alphabetically, with natural number order (`Ch 2` before `Ch 10`), unless
the user reorders a folder by hand. Manual order and folder appearance live in
`.jot/layout.yaml`, so reordering never changes a note file:

```yaml
order:
  Ashfall/Chapters:
    - Prologue.md
    - The Ferry.md
    - Ch 2.md
folders:
  Ashfall:
    emoji: 📚
    icon: book
    color: pink
pinned:
  - Inbox/Pancakes.md
```

- Paths are relative to the vault root and use `/`.
- `emoji` is shown before the notebook's name.
- `pinned` notes stay at the top of the note list, in whatever order it is in.
- Renaming or moving a note or notebook in Jot takes its entries with it.
- Entries that no longer exist are ignored and pruned on the next write. Files missing from a
  list sort after the listed ones, alphabetically.
- Jot keeps what it does not use as written: other sections, and values nested under a
  notebook. Comments inside the sections it writes are not kept. A file Jot cannot read is left
  alone, and changes to it are refused until it is fixed.
- When sync finds this file changed on both sides, Jot merges it key by key.

## Vault settings

`.jot/vault.yaml` holds settings that belong to the vault rather than to one computer:

```yaml
format: 1
attachments: per-note # per-note, per-folder or vault
attachments-folder: attachments # for per-folder and vault layouts
links: markdown # markdown or wikilink, for note-to-note links
inbox: Inbox.md # where quick add puts to-dos, as a checkbox at the end
```

Jot refuses to write to a vault whose `format` is newer than it understands, and says so.

## Importing from Joplin

A JEX export or Joplin's database maps onto this format as follows:

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
| Encrypted items, revisions          | Left out, and counted in the import's summary                |

Importing again is safe, so a Joplin account can come across one notebook at a time.
`.jot/joplin.yaml` records which file each Joplin note and notebook became, and what each note
held when imported. A later import of the same or an overlapping export then:

- leaves alone notes that Joplin has not changed;
- brings up to date notes changed in Joplin but not in Jot;
- leaves as they are notes edited in Jot since, and says so;
- does not bring back notes removed in Jot, and finds ones moved without edits;
- moves a notebook imported without its parent under that parent, once the parent is imported.

Deleting `.jot/joplin.yaml` only makes the next import treat everything as new.
