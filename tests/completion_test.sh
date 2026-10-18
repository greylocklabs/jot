#!/bin/bash
# Checks the bash completion script against the built jot:
# what it offers to put in place of the word being typed.
#
#   tests/completion_test.sh data/completions/jot.bash build/src/cli/jot
set -u

script="$1"
bin="$(mktemp -d)"
trap 'rm -rf "$bin"' EXIT
ln -s "$(realpath "$2")" "$bin/jot"
export PATH="$bin:$PATH"
# shellcheck disable=SC1090
source "$script"
failures=0

# Offers for `line`, with the cursor at its end, as bash would ask.
offers() {
    COMP_LINE="$1"
    COMP_POINT=${#COMP_LINE}
    COMP_WORDS=()
    COMP_CWORD=0
    _jot
    printf '%s\n' "${COMPREPLY[@]}"
}

expect() {
    local got
    got="$(offers "$1")"
    if ! printf '%s\n' "$got" | grep -qxF -- "$2"; then
        printf 'FAIL: for "%s" expected "%s" among:\n%s\n\n' "$1" "$2" "$got"
        failures=$((failures + 1))
    fi
}

expect "jot do" "doctor"
expect "jot logs --fo" "--follow"
expect "jot completions " "zsh"
# After = or :, bash puts only what follows it in place.
expect "jot --verbose up" "upgrade"
expect "jot don" "donate"
expect "jot donate --pr" "--print"

# Escaped and quoted spaces, against a stand-in jot offering one name, as note names will be.
jot() { printf '%s\n' "$candidate"; }
candidate="Work: team"
expect 'jot add x --notebook Work:\ t' '\ team'
expect 'jot add x --notebook "Work: t' 'Work: team'
candidate="--notebook=Deep work"
expect 'jot add x --notebook=Deep\ w' 'Deep\ work'
expect 'jot add x --notebook="Deep w' 'Deep work'
# Without : as a word break, bash replaces the whole word.
candidate="Work: team"
COMP_WORDBREAKS=${COMP_WORDBREAKS//:/} expect 'jot add x --notebook Work:\ t' 'Work:\ team'

if [ "$failures" -gt 0 ]; then
    printf '%s check(s) failed\n' "$failures"
    exit 1
fi
printf 'all completion checks passed\n'
