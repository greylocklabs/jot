#!/bin/sh
# Renders the eight screenshots www/index.html shows into www/screenshots, from a release
# build ($1, jot-gui) and the sample vault, so the same commit renders the same pictures but
# for the day: the sample to-dos move to the day of the render, as if written then.
set -eu

gui=$1
out=www/screenshots
scratch=$(mktemp -d)
trap 'command rm -rf "$scratch"' EXIT

# A vault named Notes, as most people's is, and an empty home for Jot's settings and index.
cp -r tests/data/sample-vault "$scratch/Notes"
for dir in config data state cache; do
    mkdir -p "$scratch/xdg/$dir"
done
# The sample's to-dos are due around 2026-10-27; every date moves by the days since then.
python3 -I - "$scratch/Notes" <<'PY'
import datetime, pathlib, re, sys
shift = datetime.date.today() - datetime.date(2026, 10, 27)
def moved(m):
    return (datetime.date.fromisoformat(m.group(0)) + shift).isoformat()
for note in pathlib.Path(sys.argv[1]).rglob("*.md"):
    if ".jot" in note.parts:
        continue
    text = note.read_text(encoding="utf-8")
    note.write_text(re.sub(r"\b\d{4}-\d{2}-\d{2}\b", moved, text), encoding="utf-8")
PY

shot() {
    name=$1
    shift
    XDG_CONFIG_HOME="$scratch/xdg/config" XDG_DATA_HOME="$scratch/xdg/data" \
        XDG_STATE_HOME="$scratch/xdg/state" XDG_CACHE_HOME="$scratch/xdg/cache" \
        QT_QPA_PLATFORM=offscreen "$gui" --size 1280x800 --vault "$scratch/Notes" "$@" \
        --screenshot "$out/$name.png"
    echo "$out/$name.png"
}

shot crossings-dark --note Ashfall/Crossings.md --theme callie
shot crossings-light --note Ashfall/Crossings.md --theme callie-light
shot ferry-dark --note "Ashfall/Places/The Ferry.md" --theme callie
shot ferry-light --note "Ashfall/Places/The Ferry.md" --theme callie-light
shot todos-dark --tasks --note Ashfall/Crossings.md --theme callie
shot todos-light --tasks --note Ashfall/Crossings.md --theme callie-light
shot theme-paper --note Ashfall/Crossings.md --theme paper
shot theme-tomorrow-night --note Ashfall/Crossings.md --theme tomorrow-night
