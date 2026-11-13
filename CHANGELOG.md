# Changelog

What changed in each version of Jot, for the people using it. Notes collect under Unreleased
as changes land, and `make release` dates them; see [RELEASING.md](RELEASING.md).

## Unreleased

### Breaking

- A note is no longer a task: a to-do is a checkbox. `task:`, `due:`, `done:`, `repeat:` and
  `priority:` in front matter are ordinary keys, and the Task toggle is gone. A Joplin to-do
  imports as a checkbox at the top of its note, with its day and when it was done.
- Favorites replace pins. Pinned notes are no longer read from `.jot/layout.yaml` or sorted first;
  favorite them again from the note's menu. `jot ls --json` says `favorite` in place of `pinned`.
- Find and replace moves to Ctrl+H, leaving Ctrl+R to vi's redo.

### Added

- Favorites, under Tasks in the sidebar: add a note from its menu, the palette or by dropping it
  there, drag them into your own order, and right-click to change the star.
- `tasks: off` in a note's front matter keeps its checkboxes out of Tasks, reminders and the CLI.
- Hide notes and notebooks from the lists without ignoring them, from their menus; Hide and Ignore
  sit side by side and say what each does.
- Each notebook remembers its own sort, and Settings sets the default for the rest.
- Line numbers in the left margin, from the toolbar, with the cursor's line in the accent color.
- Toolbar row two holds the modes, the outline, line numbers, a warning sign when front matter is
  invalid, and a red trash button that asks first. When saving by hand, a Save button joins them.
- Front matter completes keys and values as you type, marks the default value, and offers a month
  to pick `created:` from.
- Vi mode has a `:` command line (`:N`, `:$`, `:w`, `:q`, `:wq`, `:s`, `:%s`, `:noh`) that offers
  commands as you type, and `:help` opens the vi keys.
- Right-click a note for an edit menu with spelling, undo, cut, copy, paste and a Format submenu,
  and right-click a to-do in Tasks for its actions.
- Reference links, `[text][label]` with `[label]: Note.md` below, show as links, follow on click,
  count as backlinks and are kept up to date on renames.
- Sync shows as in Callie: a round button that turns while syncing, beside a status pill that says
  when the vault last synced and lists what is waiting.
- Ctrl+Shift+N makes a notebook.
- Settings can hide the word and character counts under notes; a note's own target still shows.
- Joplin imports run in the background, keeping notes made or edited meanwhile as they are.

### Changed

- Renaming a note can be undone and redone, from the note or the title field.
- The closing line of front matter shows, with room below it before the note.

### Fixed

- Global quick add brings Jot to the front on Wayland.
- The vi status line gives way as the window narrows instead of overlapping.
- A notebook's emoji is no longer cut off by the expand arrow.
- Completions show only while typing, and in vi only in insert mode.
- Sync buttons, Connect and Commit wait while a sync or a check runs.
- A line break before a list marker no longer doubles the marker.
- Line numbers keep clear of folded lines and drawings.

## 0.5.9 - 2026-11-12

### New

- Hold back or forward to pick a note from the trail
- Write and read only markdown links
- Switch data files between styled, source and read
- Keep this computer's settings in local.ini
- Change keyboard shortcuts in place

### Fixed

- Keep # and ? in note names out of link anchors and queries
- Read a data file kept as text again when its mode is picked
- Read hand-written shortcuts and keep fixed keys taken

## 0.5.8 - 2026-11-12

### New

- Play videos where they are embedded
- Keep the app's words in qt translation files

### Fixed

- Build the sync choices so qmlformat can lay settings out
- Embed attached videos so they play in place
- Icons, room and a themed seek bar for the media players
- Keep text positions inside the note when placing things

## 0.5.7 - 2026-11-11

### New

- Move through to-dos, the trash and the sidebar with vi keys
- Lay the outline over the note on a narrow window
- Show a tag's notes and to-dos together

### Fixed

- Say when a failed file is tried again when syncing by hand
- Show an empty date as something to press, not grayed out
- Let the floating outline take escape, not the whole window
- Give the note the keys back however the outline goes
- Hand the keys back only when the outline took them
- Keep the word count full width under a floating outline
- Keys, sort order and tag links follow the tag view
- Leave the tag view from notebook menus and search
- Draw code's surface behind it, so selecting never thickens it

## 0.5.6 - 2026-11-11

### New

- Set git sync's commit message, hooks and signing
- Rebase onto what git sync fetches, or merge
- Commit and push to git by hand when the vault says
- Set how often sync runs, or only when asked

### Fixed

- Rebase only a clean tree, so nothing set aside can clash
- Read uncommitted changes without git's index lock
- Count files renamed upstream as changed there
- Write whole numbers exactly and cap the sync interval

## 0.5.5 - 2026-11-11

### New

- Ignore paths by vault patterns and .gitignore files
- Adopt a vault's own git origin however it is written
- Find and replace in the note with ctrl+f and ctrl+h

### Fixed

- Escape ignored notebook names and keep .jot in git sync
- Keep tokens out of the origin shown in git errors

## 0.5.4 - 2026-11-11

### Added

- Double-click a note in the list to rename it there. Rename, from a note's menu or the keyboard,
  does the same, bringing the list back if it was put away.
- What's new shows a release's breaking changes first, in red.
- Double-click a to-do in Tasks to set its day and repeat, as Ctrl+Shift+D does in its note; a
  task note's go in its front matter.

### Changed

- The notebooks and the note list put themselves away together as the window narrows, and come
  back together, as the title bar's button shows or hides both. Shown on a narrow window, they lie
  over the note instead of squeezing it, and go again with a click on the note or a note picked.
- A data table's column headings look clickable, lighting up under the pointer with a tip that says
  a click sorts, and the column sorted by shows an arrow for which way.

### Fixed

- Renaming a notebook types the new name where the old one was, in a field the shape of its row.
- A data table's tools, Undo and Redo among them, wrap onto more lines on a narrow window instead
  of being cut off.
- In vi mode, the status line hides while a data file shows as a table, and comes back with its
  text.

## 0.5.3 - 2026-11-11

### Breaking

- To-dos no longer read Obsidian Tasks' emoji markers (📅, 🔁, ✅): write `@due`, `@weekly` and the
  other plain markers, and `@done`, instead.
- @labels are gone: an `@word` on a to-do is now just a word. Use tags.

### Added

- To-dos have a priority, as in Todoist: `@p1` urgent, `@p2` high, `@p3` medium and `@p4` low, or
  `priority: p1` on a task note. Quick add reads a bare `p1` too. The task list flags each in its
  color and filters by priority, Read mode says 🚩 Urgent,
  and `jot tasks --priority p1` narrows the list.
- Settings has a Tasks tab, in place of Reminders, with the reminders and a choice of how the
  to-do list is ordered: by due date, when the note was made, name or priority, either way round.
- A syntax guide in the help menu, below Keyboard shortcuts, explains what Jot reads in a note:
  links and embeds, tags, to-dos and their markers, front matter, tables, math, diagrams and
  CriticMarkup.

### Changed

- To-dos say when they are due, how they repeat and when they were done in plain words instead of
  emoji: `@due 2026-11-02 14:00`, `@daily`, `@weekly`, `@monthly`, `@yearly`, `@weekdays`,
  `@every 2 weeks on monday` and `@done 2026-11-01`. Days typed another way, as `@due tomorrow 3pm`,
  `@due 11-14-2026` or `@done yesterday`, and `@done` alone, become dates when you leave the line, and a to-do said
  done is ticked. Typing `@` on a to-do offers them, with
  today, tomorrow and next week as days. The editor colors each kind its own way, and Read mode
  shows them as 🔁 weekly, 📅 Nov 2 and ✅ Nov 1.
- A task note can repeat, with `repeat: every week` in its front matter: completing it moves its
  due day on instead of finishing it.

### Removed

- @labels on to-dos and task notes, with the Labels section of the sidebar, the label filter
  and `jot tasks --label`. Tags are the one way to sort notes and to-dos; an `@word` is now just
  a word.

## 0.5.2 - 2026-11-10

### Added

- The notebooks and the note list put themselves away as the window narrows, the notebooks first,
  and come back as it widens. A button at the left of the title bar, or Ctrl+\, puts them away or
  brings them back by hand.
- In vi mode, what a yank takes flashes in the selection's color for half a second.
- In vi mode, f, F, t and T reach past the line, as in flash.nvim: the character's other matches
  that way are marked, and pressing f, F, t or T again steps on or back through them.

### Changed

- The format row's record button is a microphone, and a stop square while recording.
- Formatting buttons that do not fit the format row go into a more menu at its end, instead of
  wrapping onto more lines.

### Fixed

- On a narrow note, Delete becomes a trash can and the title gets a little smaller as needed, so
  the title keeps its words whole.

## 0.5.1 - 2026-11-09

### Added

- Source mode highlights the Markdown as written: heading, list and task marks, emphasis, links,
  front matter, and code by its language, with every mark shown and nothing resized.
- A sound embedded on a line of its own plays in place in Read mode, as in the editor.
- A notebook can have a word target and a deadline, set from its menu. Under its notes, the footer
  shows the words so far and how many a day it takes to make the deadline.

### Changed

- Settings has Writing and Reminders tabs, so each tab fits without scrolling.

### Fixed

- Exporting a note keeps the first word of each footnote.
- Restore is off, saying why, for a past version the same as the note now.
- The outline's headings line up under its title.
- Read mode draws checkboxes like the rest of Jot's, with ticked tasks struck through.
- On a narrow window the format row moves its switches below the formatting buttons instead of
  piling the buttons into a column, and a long notes folder name is cut short in the title bar.

## 0.5.0 - 2026-11-09

### Added

- A setting to save notes only on Ctrl+S instead of as you type. A dot by the title marks changes
  not yet saved, and Jot asks before leaving a note or quitting with any.
- Typing a colon and part of an emoji's name, as `:fox`, offers matching emoji to put in, chosen
  with the arrows and Return like wikilinks.
- A `${toc}` line, as Joplin writes it, shows the note's headings as links, kept up to date as
  they change, in the editor and when reading.
- An outline of the note's headings beside it, from the format row or Ctrl+Shift+O, marks the
  one the cursor is under and goes to any of them.
- Tabs: a middle click or Ctrl+click on a note, or Open in a new tab from its menu, opens it in a
  tab of its own. Ctrl+Tab and Ctrl+Shift+Tab move between tabs, and Ctrl+W closes one.
- Templates: notes in a Templates notebook make new notes from a notebook's menu or the command
  palette, with the title, date and time filled in. Today's note, from the palette, opens the
  day's note in a Daily notebook, made from a Daily template when there is one.
- Word and character counts under each note, for the selection too, with progress toward a
  `target:` in its front matter, the words written today and a daily goal's streak, set in
  Settings.
- Focus mode fades all but the paragraph being written, typewriter scrolling keeps its line in
  the middle of the window, and F11 goes fullscreen with only the note.
- CriticMarkup edits show as additions and deletions, and its comments as notes in the margin
  beside their lines, where the window has room.
- Export a note to DOCX, ODT, PDF, ePub or HTML through Pandoc, from its menu or the command
  palette, in Pandoc's own look or standard manuscript format (Shunn).
- Spellcheck underlines words the dictionary does not know, with suggestions on a right click,
  in the desktop's language, a vault's or a note's own.
- Record from the microphone into a note, from the format row or the command palette. A sound
  embedded on a line of its own plays there.
- LaTeX math, `$...$` in a line and `$$` on lines of their own, is drawn off the cursor's line
  and when reading, through MicroTeX.

### Fixed

- Pictures added by dropping or pasting a file show when reading the note.

## 0.4.1 - 2026-11-07

### Added

- The links to a note and its mentions without a link fold under their headings, open to begin
  with.
- Trash can permanently delete a note, or empty itself, each after asking.
- Searching notes also finds those in the trash, listed under In the trash; choosing one shows it
  in Trash. A setting leaves the trash out. Trash has a search of its own, and right-clicking a
  note there restores or permanently deletes it.
- Ctrl+Z and Ctrl+Shift+Z outside the editor undo and redo moving a note to the trash or bringing
  it back, as well as ticking to-dos. One that would move a note to the trash asks first.
- Moving a notebook to the trash can be undone and redone too, bringing it back with everything
  left in it.
- Resting on a button that is off says why, as on Edit as table when the text is no list of
  records, or New note in Trash.

### Changed

- Trash sits at the foot of the sidebar with a 🗑️ emoji, which can be changed or removed from its
  right-click menu.
- Tasks has a ✅ emoji in the sidebar, which can be changed or removed from its right-click menu as
  Trash's can.
- The + button makes a note straight away, as Ctrl+N does; a note's extension can be changed
  after, and data files are still in a notebook's right-click menu. In Trash it is off, as there
  is no notebook for the note to go in.
- The Vi status line is flat, with only the mode on a color of its own, so nothing on it looks
  like a button.
- The Vi status line says how far through the file the cursor is, from 0% on the first line to
  100% on the last, rather than All, Top or Bot.
- Jot keeps deleted notes in its own trash, in the vault's .jot/trash folder, rather than the
  desktop's. A setting under General either keeps them until they are deleted or hands them to the
  desktop's trash after 30 days.
- Settings lists its tabs down the left, under its title, and keeps one size, scrolling a tab only
  when the window is too small for it.

### Fixed

- Choosing Trash no longer leaves the notebook or tag chosen before highlighted, and the trash
  list's rows are evenly padded.
- Search results show when each note last changed, rather than 12/31/69.
- In vi mode, leaving Insert no longer can leave a second cursor drawn on the note.

## 0.4.0 - 2026-11-07

### Added

- A JSON or YAML file holding several lists of records, under keys or in several YAML documents,
  opens as a table with a tab for each list. An edit in a tab rewrites only that list, so the rest
  of the file stays as it was.
- Trash in the sidebar lists the notes deleted from the vault, from the desktop's trash and Jot's
  own, newest first. Read one there, or restore it where it was, with its attachments. Jot never
  empties the trash.
- With vi mode on, a status line across the foot of the editor shows the mode in caps and in its
  own color, the file and its kind, the line and column, how far down the view is, and the day
  and time.

### Changed

- Removing a table row or column asks first: its X turns into a trash can in the same place, and
  a second click removes it.
- Diagrams take every color from the theme: timeline sections, pie slices, chart series and git
  branches come in the theme's sticker colors, with text that reads on each.

### Fixed

- Shift and the mouse wheel scroll a table across over its column and row headers too, not only
  over its cells.
- A wide table's bar for scrolling across sits right under its last row, not at the bottom of the
  window, where it was often out of view.
- Double-clicking a table column's heading to rename it no longer sorts the table twice, which
  could close the name field as it opened. A single click still sorts.

## 0.3.0 - 2026-11-06

### Added

- JSON, CSV, TSV and YAML files in a vault show in the note list with their extension, count in
  their notebook, turn up in search and the switcher, and open in Jot from a link, to edit as
  text.
- Data files are colored by their kind, JSON and YAML by their syntax and CSV and TSV column by
  column, with line numbers and the bracket by the cursor matched to its partner. When one stops
  reading as its kind, a line above it says where and why, and a click goes there.
- A JSON, CSV or TSV file holding a list of records opens as a table: a row per record and a
  column per field. Edit cells in place, sort by a column, filter rows, add and remove rows and
  columns, rename columns, and undo. JSON keeps its numbers, booleans and nulls, and nested values
  open in an editor of their own. Saving rewrites only what changed, so the rest of the file keeps
  its formatting, key order, quoting and line endings. Values that differ in kind from the rest
  of their column are marked. "Edit as text" shows the file as it is.
- A data file with a JSON Schema beside it, `<name>.schema.json` or the file its `$schema` names,
  is checked against it: the table marks each value that breaks it, says why, and offers the
  values the schema lists while you type. `jot check data/*.json` does the same from the command
  line, printing `file:line:column: problem` and exiting non-zero, for CI.
- Mermaid diagrams in a `mermaid` code block are drawn in place, in the theme's colors and the
  note's font, in the editor and in Read. Put the cursor in the block to edit its text. A diagram
  with a mistake in it stays as code. Drawn by [Pele](https://pele.run), inside Jot: nothing is
  fetched from the web.
- A YAML file holding a list of mappings opens as a table too. Edits keep its comments, key
  order and quoting wherever the values did not change, and nested lists and mappings edit as JSON.
- New, in the title bar, and New file, in a notebook's menu, make a JSON, CSV, TSV or YAML file
  that starts with one example record, so it opens as a table. A new note still empty, named
  `people.csv`, becomes a CSV file the same way.
- Ctrl+Q quits Jot, and F5 syncs.
- Settings, Developer: Developer mode shows each note's file path under its title, to copy, and
  Log debug details writes Jot's debug lines to its log, as in Callie.

### Changed

- Back and forward sit beside the title, and the title bar's buttons are spaced as in Callie.

### Fixed

- Delete, and Edit as table, line up with the first line of a note's title.

## 0.2.0 - 2026-11-03

### Added

- Sync a vault with another folder, such as a NAS, a USB drive or a network share, and so with
  other computers syncing with it: Settings, Sync. Jot syncs when it opens, shortly after you
  write, and every few minutes; the title bar shows how it stands, and a click syncs now. Edits
  made on both sides are merged, front matter key by key, or both versions kept; a delete never
  wins over an edit, and what a sync deletes goes to the vault's trash.
- Sync with an S3-compatible bucket too, such as on AWS, Cloudflare R2, Backblaze B2, Wasabi or
  MinIO: Settings, Sync, Sync with S3. Jot checks the bucket first, and keeps the key in your
  desktop's keyring.
- Sync with a git repository, on GitHub, GitLab, Codeberg, your own server or a NAS over SSH:
  Settings, Sync, Sync with git. Jot reaches it as git does from a terminal, with your SSH keys
  or saved logins, commits each sync, and keeps pictures, PDFs, audio and video in Git LFS.
  Edits clashing in a note are merged or kept as a conflict copy, never left as conflict
  markers.
- `jot sync` syncs the vault once from the command line.
- When another version of a note was kept beside it, as when two computers changed the same
  lines, the note says so, and Compare shows the two side by side with what differs marked, to
  keep this one or use the other. Either way, what is let go can be brought back.
- The same goes for notes in a folder that Syncthing, Nextcloud, ownCloud or Dropbox syncs: the
  copies they keep when two versions clash show beside the note, to compare and resolve.
- Past versions show what changed, line by line, against the note now or any other version,
  with the whole text a click away. Old versions thin out as they age: all from the last day,
  then one an hour for a week, one a day for a month, and one a week after.
- Add a to-do from anywhere: turn on Settings, Keyboard, Quick capture, and a system-wide key
  (Ctrl+Alt+J unless your desktop gives another) opens quick add, even with Jot's window
  closed. Or bind any key to `jot-gui --quick-add` in your desktop's shortcut settings.

### Fixed

- Menus are as wide as their entries need, so the task view's menu in a narrow column shows
  Today, Upcoming and the rest rather than blank or cut-short entries.

## 0.1.2 - 2026-11-03

### Added

- Vi mode's f, F, t and T go along the line to the next character typed, ; repeats the last one,
  and a comma repeats it the other way when the leader is another key. They select in visual
  mode, and d or c before them cuts or changes up to that character.
- A Done view in Tasks lists the to-dos ticked off and the task notes completed, most recently
  first, under the day they were done; unticking one there opens it again.
  `jot tasks --view done` lists them too.
- Back and forward through the notes opened, as in a browser: Alt+Left and Alt+Right, a mouse's
  back and forward buttons, the arrows in the title bar, or Ctrl+O and Ctrl+I in vi mode. Going
  back returns to where the cursor was, and the way follows notes renamed or moved and passes
  over those trashed.
- Ctrl+Shift+D sets the day and repeat of the to-do at the cursor:
  words in it such as "water the plants every week" or "call Soren tomorrow 3pm" become its
  markers, as quick add writes them, and with none, a popup takes them typed or a day picked.
  Emptied, it takes them away. A line that is no to-do becomes one.

### Changed

- Ticking a to-do writes the day it was done after it, as `✅ 2026-11-02`, as Obsidian Tasks
  does, and unticking it takes that away.

### Fixed

- The task view's switch and + fit a narrow middle column: the switch takes shorter labels, and
  at its narrowest becomes a menu, rather than being cut off.

## 0.1.1 - 2026-11-02

### Fixed

- The task view's Today, Upcoming, Anytime and All switch lines up with the + beside it.
- Editing a note's `title:` updates the title above it, and clicking that title no longer puts the
  old one back.
- Holding h, l or another key with accented letters in vi normal or visual mode keeps moving,
  rather than waiting to offer the accents.
- Ctrl+Z and Ctrl+Shift+Z undo and redo a to-do ticked, or a task note completed, from the task
  view. Ticking the note shown keeps the editor's own undo, and ticking a box in a note is one
  step to undo.

## 0.1.0 - 2026-10-31

### Added

- Import from Joplin reads the Joplin app on this computer when it is there, so nothing needs
  exporting first; a .jex export still works. `jot import joplin` with no file does the same.
- Joplin's note history comes across too: importing from the Joplin app brings each note's
  revisions in as past versions, at the times Joplin kept them.
- Past versions: Jot keeps earlier versions of each note, one when you start editing it and then
  one every ten minutes while you write. Open them from a note's menu, the command palette, or
  the leader key then h, read any of them, and restore one; what it replaces is kept too. They
  are plain files in the vault, under `.jot/history`, so they sync and back up with your notes.
- Reminders: a to-do notifies when it comes due, at its time, or at 9:00 on its day when it has no
  time; the morning time is in Settings. The notification opens the to-do, ticks it done, or
  snoozes it for 10 minutes. Those that came due while Jot was closed are told when it starts.
- Jot keeps running in the tray when its window closes, so reminders still come, and can start
  when you log in. Opening Jot again brings back the window rather than a second Jot.
- To-dos can repeat: `🔁 every week`, `every 2 weeks`, `every monday`, `every weekday` and the
  like, or "every week" in quick add. Ticking one writes the next one, due on its next day.
- Quick add: press Ctrl+Shift+A, or the + in Tasks, and type a to-do as you would say it, "call
  Soren tomorrow 3pm @minutes #calls" or "water the plants every week". Jot shows the day, time,
  repeat and labels it read, then adds a checkbox to the Inbox note, which `inbox:` in
  `.jot/vault.yaml` can name. `jot add` does the same from the command line.
- Tasks has Today, Upcoming, Anytime and All views, as a to-do app does. A checkbox can say when
  it is due and give itself labels, in the format of Obsidian's Tasks plugin:
  `- [ ] Call Soren @minutes 📅 2026-10-29 14:00`. Labels are listed in the sidebar, and choosing
  one, or a tag while in Tasks, narrows the list. Task notes take labels in `labels:`. The badge
  on Tasks counts what is due today or overdue.
- `jot tasks` takes `--view`, `--label` and `--tag` to match. Its JSON names a to-do's words
  `title` rather than `text`, and adds `note`, `due`, `repeat`, `labels` and `tags`.
- A reading view: choose Read beside Styled and Source, or press Ctrl+Shift+R, to see the note
  drawn as it reads, with tables, quotes, code and images. Links, wikilinks and tags open what
  they lead to, and a click on a checkbox ticks it. Formatted is now called Styled, to make room.

### Changed

- When a note changes elsewhere while you are typing in it, such as from a sync, Jot merges the
  two when they touch different lines, and the cursor stays where you were. Only where both
  changed the same lines is the other version kept aside as a conflict copy, as before.
- Callie Light's pink is deeper, with white text on it, so buttons and what is selected are
  easier to read.

### Fixed

- Renaming or moving a note keeps links to its attachments working, in the note itself and in
  any other note that links straight to one of them.
- Moving a note to the trash asks first when other notes link to its attachments, and keeps
  them unless you choose to trash them too. The editor's Delete asks the same.
- A note with both Windows and Unix line endings keeps each line's own when saved, rather than
  every line taking Windows endings; new lines take the kind most of the note uses.
- A note's attachments folder left empty, as when its files were deleted in a file manager, is
  removed when Jot opens the notes folder or the note.

## 0.0.12 - 2026-10-27

### Added

- Notebooks can have an emoji, shown before their name in the sidebar and wherever notebooks are
  listed. Choose one from a notebook's menu, by typing words of its name. It is kept in the
  notes folder's `.jot/layout.yaml`, so it syncs with the notes, and notebooks imported from
  Joplin keep theirs.
- The note list can be ordered by when notes were made, as well as by when they changed or by
  title, and `jot ls --order created` does the same.
- Pin a note from its menu, or from the command palette, to keep it at the top of the list. Pins
  are kept in `.jot/layout.yaml` too, and `jot ls` lists pinned notes first.
- Nested tags, such as `#novel/ch1`, show as a tree in the sidebar that folds like the notebooks,
  each with how many notes it covers. A tag that others are nested in shows even when no note
  has it itself, and so does `jot tags`.

## 0.0.11 - 2026-10-27

### Added

- A command palette: press Ctrl+Shift+P, or the leader then c in vi mode, and type a few letters of
  any command to run it, including those only in menus. Commands that cannot run at the moment
  are left out.

### Changed

- Choosing a file or folder opens your desktop's own file chooser, with its places and
  bookmarks, rather than a plain one of its own.

## 0.0.10 - 2026-10-27

### Added

- Import from Joplin: choose a Joplin export (.jex) from Settings › Notes folder, or from a
  notebook's menu to import into it. Jot says what it will bring across before it does. Notebooks
  become folders, and tags, to-dos, links and attachments come across. Importing again only brings
  what changed in Joplin, never over your edits in Jot. Where Joplin recorded a note being written
  is left out.
- `jot import joplin <export.jex>` does the same from the command line, with `--dry-run` to see
  what it would do first.
- `jot tags` lists every tag, with how many notes have it.

## 0.0.9 - 2026-10-25

### Fixed

- The date picker's month arrows go to the next and previous month again; west of UTC, Next stayed
  put and Previous skipped a month.

## 0.0.8 - 2026-10-19

### Added

- "Report a problem" in the ? menu files a bug or an idea as a public GitHub issue without a GitHub
  account, and shows the details it includes before you send them.
- `jot donate` opens Jot's Ko-fi page, and `--print` prints its link instead.

## 0.0.7 - 2026-10-17

### Added

- The installer adds the Greylock Labs repository at <https://packages.greylocklabs.org>, so Jot's
  updates come with `dnf upgrade` or `apt upgrade`.

## 0.0.6 - 2026-10-17

### Changed

- Jot has its own website, [jotwriter.org](https://jotwriter.org), and its installer moved there:
  `curl -fsSL https://jotwriter.org/install.sh | sh`. The old addresses on callieapp.org keep
  working.
- Jot's code, issues and releases moved to [github.com/greylocklabs/jot](https://github.com/greylocklabs/jot).
  The About dialog, problem reports and `jot upgrade` use the new addresses.

## 0.0.5 - 2026-10-16

### New

- Settings, Theme, Note text: write notes in the theme's font, a serif or a mono one.
- Code blocks take their colors from the theme, worked out from its accent, so they match it.
  A theme file can set them, and the note font, in a new `[editor]` section.
- Notebooks can be made, renamed and moved without a file manager: the + beside Notebooks makes
  one, a double-click renames one, and dragging a note or a notebook onto another moves it there.
  Links to the notes inside keep working.
- Right-click a note for Open, Rename, Move to, Copy link, Show in Files, Make a task and Move
  to trash, or a notebook for a new note or notebook inside it, Rename and Show in Files. The
  Menu key opens the selected note's.
- The sidebar and the note list can be made wider or narrower by dragging their edges; a
  double-click puts one back. While a note or notebook is dragged, a copy of it follows the
  pointer.
- The format row's Formatted and Source buttons switch between styled and as-written Markdown,
  its Task switch makes the note a task, and its H button holds the three heading sizes. Done is
  a checkbox.
- Delete moves the selected note, or the notebook clicked in the sidebar, to the trash after
  asking; a notebook's menu offers the same. A notebook goes with everything in it.
- Vi mode, under Settings, Keyboard. Notes open in normal mode, where h, j, k and l move, x, dd
  and p edit and u undoes; i, a or o start typing and Esc stops, and v selects. Out of a note,
  j and k go through the list, and / searches it: Enter goes to what was found, Esc clears
  the search and leaves it. A leader key, comma by default, starts commands such as a new
  note or the tasks, and shows what can follow it if the next key is slow to come.
- The keyboard shortcuts list (F1) is made from the same table as the shortcuts themselves,
  vi mode's keys included.

### Changed

- To-dos are now tasks, everywhere: the sidebar's Tasks, the Task button, and `jot tasks`. A
  note that is a task says `task: open` or `task: done` in its front matter; `todo:` is no longer
  read.

### Fixed

- The keyboard shortcuts dialog shows again.
- The fold arrow beside a notebook comes to a clean point.
- A long note opens at its title, not scrolled part way down.
- The sidebar leaves room under the title bar.
- `jot upgrade` on a computer without Jot's package names Jot's installer, not Callie's.

## 0.0.4 - 2026-10-15

### New

- Lists carry on as you type: Return starts the next bullet, number or unticked box, Return on an
  empty item ends the list, and Tab and Shift+Tab nest and unnest items. Shift+Return breaks the
  line without a new item.
- Tables: the format bar's table button adds one, or lines up the one you are in. Tab and
  Shift+Tab move between cells, padding them so the columns align; Tab in the last cell and
  Return add a row, and Return on an empty last row ends the table. Tables show in a monospace
  font so the columns line up on screen too.
- Typing `[[` offers your notes as you go, by title, alias or folder. Arrows choose one, Return
  or Tab puts it in and closes the link, and Escape puts the list away. When two notes share a
  title, the link names its folder too.
- Ctrl+/ or the Source button shows a note's Markdown exactly as written, in the monospace font,
  with nothing folded away; again brings the styling back. Jot remembers which you prefer.
- Code blocks are colored by their language, named after the opening fence (` ```python `,
  ` ```rust `, ` ```sh ` and hundreds more), through KDE's syntax highlighting.

## 0.0.3 - 2026-10-15

### New

- Open a folder of Markdown notes as your vault. The sidebar lists its notebooks, which fold, and
  its tags; the middle column lists their notes, newest or by title, and searches them; Ctrl+P
  finds any note by a few letters of its title, an alias or its folder.
- Write notes in Markdown that styles itself as you type: headings grow, and the marks behind
  emphasis, links and code fold away except on the line you are on. Notes save as you type.
  Ctrl+N makes a note, its title renames it and every link to it, Ctrl+click follows a link (or
  makes the note a wikilink names), and a click ticks a task.
- Paste an image, or drop files on a note, to keep them beside it in its `.assets` folder. An image
  on a line of its own shows in place of its link.
- Under each note, the lines that link to it, and the notes that name it without a link. Resting the
  pointer on a link shows the start of the note it leads to.
- To-dos, in the sidebar: notes that are to-dos, soonest due first, and every unticked box in the
  vault. Ticking one changes its note; choosing one opens the note on that line.
- `jot status`, `ls`, `search`, `new`, `show`, `open` and `todo` work on your vault from a terminal,
  with `--json` for scripts; `jot new` takes a note's body from a pipe.
- A row of formatting over each note: headings, bold, italic, strikethrough, code, links, lists,
  checkboxes, quotes and code blocks, each also undone in one step, and most with a shortcut. The
  To-do button makes the note a to-do, with a due date and a Done switch under its title.
- Settings, under the title bar's slider button or Ctrl+,: change the notes folder, pick, import,
  export or recolor a theme, and turn off the daily check for new releases. The ? menu copies debug
  info, reports a bug, lists the keyboard shortcuts, says what is new, and tells you when an update
  is out.
- `jot upgrade` and `jot downgrade` install another release, `jot logs` and `jot doctor` help with
  bug reports, and the `jot` command completes in bash, zsh and fish.

## 0.0.2 - 2026-10-14

### New

- Add an install script

## 0.0.1 - 2026-10-14

### New

- A first window with Callie's look: the shared themes, a rounded frame and its own title bar.
- `.deb` and `.rpm` packages for Fedora, Debian and Ubuntu.
