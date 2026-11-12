#!/bin/sh
# Fails when qmlformat cannot lay out a QML file: it then says so, over and over, only to a
# terminal unless told otherwise, and leaves the rest of the file wrongly indented.
#
#   tests/qmlformat_test.sh /usr/lib64/qt6/bin/qmlformat src/ui
set -u

qmlformat="$1"
status=0
for file in "$2"/*.qml; do
    said="$(QT_FORCE_STDERR_LOGGING=1 "$qmlformat" "$file" 2>&1 >/dev/null | grep -c 'Unhandled state')"
    if [ "$said" -gt 0 ]; then
        echo "qmlformat cannot lay out $file ($said times)" >&2
        status=1
    fi
done
[ "$status" -eq 0 ] && echo "qmlformat reads every file"
exit "$status"
