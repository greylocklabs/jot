#!/usr/bin/env bash
# Checks that every place naming the version agrees: CMake, the RPM spec, the
# Debian changelog, the AppStream metainfo and CHANGELOG.md, whose release
# date the metainfo must also carry. With an argument, such as a tag's
# version, they must also match it. Before the first release, when neither
# CHANGELOG.md nor the metainfo has one yet, only CMake and the packaging are compared.
#
#   scripts/check-version.sh          # agree with each other
#   scripts/check-version.sh 0.1.0    # and with 0.1.0
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
app="$(sed -n 's/^project(\([a-z0-9-]*\).*/\1/p' "$root/CMakeLists.txt" | head -1)"
app_id="$(sed -n 's/^set([A-Z_]*_APP_ID "\([^"]*\)".*/\1/p' "$root/CMakeLists.txt" | head -1)"
cmake="$(sed -n 's/^ *VERSION \([0-9][0-9.]*\)$/\1/p' "$root/CMakeLists.txt" | head -1)"
spec="$(sed -n 's/^Version: *//p' "$root/packaging/rpm/$app.spec" | head -1)"
debian="$(sed -n "1s/^$app (\\([^-)]*\\).*/\\1/p" "$root/packaging/debian/changelog")"
changelog="$(sed -n 's/^## \([0-9][0-9.]*\) - .*/\1/p' "$root/CHANGELOG.md" | head -1)"
released="$(sed -n 's/^## [0-9][0-9.]* - \([0-9-]*\).*/\1/p' "$root/CHANGELOG.md" | head -1)"
metainfo="$root/data/$app_id.metainfo.xml"
appstream="$(sed -n 's/.*<release version="\([^"]*\)".*/\1/p' "$metainfo" | head -1)"
appstream_date="$(sed -n 's/.*<release version="[^"]*" date="\([^"]*\)".*/\1/p' "$metainfo" | head -1)"

want="${1:-$cmake}"
status=0
pairs=("CMakeLists.txt:$cmake" "$app.spec:$spec" "debian/changelog:$debian")
# Only an app with no release yet has neither a dated section nor a metainfo release.
[ -z "$changelog$appstream" ] || pairs+=("metainfo.xml:$appstream" "CHANGELOG.md:$changelog")
for pair in "${pairs[@]}"; do
    if [ "${pair#*:}" != "$want" ]; then
        echo "check-version.sh: ${pair%%:*} says ${pair#*:}, expected $want" >&2
        status=1
    fi
done
if [ "$appstream_date" != "$released" ]; then
    echo "check-version.sh: metainfo.xml dates $want $appstream_date, CHANGELOG.md says $released" >&2
    status=1
fi
exit "$status"
