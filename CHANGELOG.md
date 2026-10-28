# Changelog

What changed in each version of Jot, for the people using it. Notes collect under Unreleased
as changes land, and `make release` dates them; see [RELEASING.md](RELEASING.md).

## Unreleased

### Changed

- Callie Light's pink is deeper, with white text on it, so buttons and what is selected are
  easier to read.

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
