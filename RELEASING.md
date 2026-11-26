# Releasing Jot

Notes for the next release collect under `## Unreleased` at the top of `CHANGELOG.md` as changes
land, in words for the people using Jot. Changes that break something people rely on, such as a file format
or a setting, go in a `### Breaking` section first, which What's new shows in red. With everything for the release on `main`:

```sh
make release-dry minor   # what it would do, changing nothing
make release minor       # or patch or major; VERSION=1.2.3 names one
```

`make release` runs only on `main`, clean and level with `origin/main`. It dates the Unreleased
notes, or has `git cliff` draft them and opens `$EDITOR` to write them up when there are none. Then
it sets the version in `CMakeLists.txt`, `packaging/rpm/jot.spec`, `packaging/debian/changelog`
and the metainfo, whose release lists the notes' first-level points, runs `make check` and
`core/tooling/check-version.sh`, and asks before it commits `chore: release X.Y.Z`, tags `vX.Y.Z` and
pushes both. When `make check` fails, it puts the version back and leaves the notes under
`## Unreleased`; notes it drafted are then uncommitted, to commit and push before releasing again.
The push goes straight to `main`, which repository admins may do.

Pushing the tag runs the release workflow. It checks the versions again, builds the source tarball
and the `.deb`, `.rpm` and Arch packages, installs each on a clean Fedora, Debian, Ubuntu and Arch
and runs it, and publishes a GitHub release with the notes from `CHANGELOG.md`. The Arch package
does not hold the release back: when it fails, the release goes out without it. Pull requests that
touch the packaging run the same builds without publishing.

After the release, the `aur` job pushes `packaging/arch/jot` and `packaging/arch/jot-notes-git`,
with the release's version and checksum filled in, to the AUR. It needs an AUR account's SSH key in
the `AUR_SSH_PRIVATE_KEY` secret; without it, it warns and skips. See
[greylocklabs/packages#11](https://github.com/greylocklabs/packages/issues/11).
