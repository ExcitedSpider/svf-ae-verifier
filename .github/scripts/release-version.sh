#!/usr/bin/env bash
# Print the next release version: the latest v<MAJOR.MINOR.PATCH>-... tag
# bumped by $1 (major, minor or fix). With no release tag yet, start from 0.0.0.
# Needs the full tag history (actions/checkout with fetch-depth: 0).
set -euo pipefail

bump="$1"
previous="$(git tag -l 'v*' | sed -nE 's/^v([0-9]+\.[0-9]+\.[0-9]+)(-.*)?$/\1/p' | sort -V | tail -1)"
IFS=. read -r major minor patch <<< "${previous:-0.0.0}"
case "$bump" in
    major) version="$((major + 1)).0.0" ;;
    minor) version="$major.$((minor + 1)).0" ;;
    fix)   version="$major.$minor.$((patch + 1))" ;;
    *) echo "unknown bump: $bump" >&2; exit 1 ;;
esac
if git tag -l "v$version" "v$version-*" | grep -q .; then
    echo "a release tag for $version already exists" >&2
    exit 1
fi
echo "release version: ${previous:-none} -> $version ($bump)" >&2
echo "$version"
