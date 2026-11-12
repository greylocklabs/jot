#!/bin/sh
# Fails when the code has words translations/jot_en.ts does not, or has lost some it does,
# so the file people edit always matches the app. `make translations` brings it up to date.
#
#   tests/translations_test.sh lupdate cmake GenerateLUpdateProject.cmake \
#       build/.lupdate/jot_lupdate_project.cmake translations/jot_en.ts
set -u

lupdate="$1"
cmake="$2"
generate="$3"
project_in="$4"
words="$5"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
# The project lupdate reads, made as the build's own update does.
"$cmake" -DIN_FILE="$project_in" -DOUT_FILE="$work/project.json" -P "$generate" ||
    { echo "could not make lupdate's project" >&2; exit 1; }
cp "$words" "$work/jot_en.ts"
"$lupdate" -silent -project "$work/project.json" -ts "$work/jot_en.ts" -locations none \
    -no-obsolete >/dev/null 2>&1 || { echo "lupdate failed" >&2; exit 1; }
if ! diff -q "$words" "$work/jot_en.ts" >/dev/null; then
    echo "translations/jot_en.ts is out of date with the code; run make translations" >&2
    diff "$words" "$work/jot_en.ts" | head -20 >&2
    exit 1
fi
echo "translations/jot_en.ts matches the code"
