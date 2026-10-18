<img src="https://raw.githubusercontent.com/greylocklabs/jot/main/assets/logo.svg"
     width="48" height="48" alt="Jot logo" />

# jot

> plain-file notes for Linux

---

Jot is a Markdown notes app for Linux, from the makers of [Callie](https://callieapp.org). Notes
are plain files in a folder you choose. Jot does not sync them itself yet: keep the folder in
Syncthing, Nextcloud or Dropbox to have it on more than one computer, and Jot picks up the changes.
Its own sync, to S3, Dropbox or Google Drive, is planned. It is built for everyday notes first, and
for writers and worldbuilders next.

Jot is at the very start. [jotwriter.org](https://jotwriter.org/) shows what it does so
far, and [docs/ROADMAP.md](docs/ROADMAP.md) has the plan.

## Installation

On Fedora 44 or later, Debian 13 (trixie) or later, or Ubuntu 26.04 or later, on x86_64:

```sh
curl -fsSL https://jotwriter.org/install.sh | sh
```

It finds the latest [GitHub release](https://github.com/greylocklabs/jot/releases), checks the
package against the checksum GitHub records for it, and installs it with `dnf` or `apt`. On any
other system it stops before changing anything. Run again, it updates Jot, or says it is up to
date. `sh -s -- --dry-run` shows what it would do, `--version 0.0.1` installs that version, and
`--force` installs it again. It also adds the
[Greylock Labs repository](https://packages.greylocklabs.org), which carries Jot and Callie, so
later updates come with `dnf upgrade` or `apt upgrade`; that page shows how to add it by hand. The
packages can also be downloaded from the release and installed by hand.

This installs the `Jot` desktop app and the `jot` command. `jot upgrade` installs the latest release,
`jot downgrade` goes back one, `jot logs` and `jot doctor` help with bug reports, and `jot donate`
opens Jot's Ko-fi page. A development build from `make run` is called Jot Devel and keeps its own
settings and folders, so it runs beside an installed Jot.

## The command line

`jot` works on the vault the app has open, or the one given with `--vault <folder>`:

```sh
jot status                       # the vault, its notes and what is left to do
jot ls Recipes --order title     # notes in a notebook, or all of them
jot search ferry tag:ashfall     # search, with tag:, in:, task: and type: filters
echo "Bring a lantern." | jot new "Ferry checklist" --notebook Ashfall
jot show "mara"                  # print a note, by title, alias or path
jot open "mara"                  # or open it in the app
jot tasks                        # task notes and unticked boxes
```

`--json` gives scripts the same, and the shell completions know your notebooks and notes.

## Development

On Fedora 44+, Debian 13+ or Ubuntu 26.04+:

```sh
make deps    # build dependencies, via dnf or apt
make setup   # dev tooling, git hooks, configure
make run     # build and open the app
make check   # what CI runs
```

Run `make` to list every command. Read [CONTRIBUTING.md](CONTRIBUTING.md) before sending a change.

## License

MIT
