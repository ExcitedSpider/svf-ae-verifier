#!/usr/bin/env bash
# Print the SVF commit for a branch, tag or commit name.
set -euo pipefail

ref="$1"
sha="$(git ls-remote https://github.com/SVF-tools/SVF.git "$ref" | head -1 | cut -f1)"
# Not a branch or tag name: treat it as a commit.
echo "${sha:-$ref}"
