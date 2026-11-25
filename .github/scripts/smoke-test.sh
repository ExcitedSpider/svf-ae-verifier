#!/usr/bin/env bash
# Smoke-test a runtime image: verify a small C program inside it, check the
# report, and check the exit status for bad usage and for a missing input.
set -euo pipefail

image="$1"
platform="${2:-}"
# Optional: the tool_version the report must carry (set by the release workflow).
expected_version="${3:-}"
run=(podman run --rm)
if [ -n "$platform" ]; then
    run+=(--platform "$platform")
fi

report="$(mktemp)"
trap 'rm -f "$report"' EXIT

"${run[@]}" "$image" sh -ec '
cat > t.c <<EOF
#include <verifier.h>
int main(void)
{
    int x = 1;
    verifier_assert(x == 1);
    int a[4];
    a[2] = x;
    return a[2];
}
EOF
clang -g -S -emit-llvm -Xclang -disable-O0-optnone -fno-discard-value-names t.c -o t.raw.ll
opt -S -p=mem2reg t.raw.ll -o t.ll
svf-verifier -property=reach,overflow t.ll
' > "$report"

jq . "$report"
jq -e '
    .status == "ok"
    and .properties.reach == "proved"
    and (.properties | has("overflow"))
    and (.sites | length) > 0
    and .svf_commit != "unknown"
    and (.llvm_version | type) == "string"
' "$report" > /dev/null
if [ -n "$expected_version" ]; then
    jq -e --arg v "$expected_version" '.tool_version == $v' "$report" > /dev/null \
        || { echo "expected tool_version $expected_version" >&2; exit 1; }
fi

# ILP32: amd64 images must compile and verify a -m32 input.
arch="$("${run[@]}" "$image" dpkg --print-architecture)"
if [ "$arch" = amd64 ]; then
    "${run[@]}" "$image" sh -ec '
cat > t.c <<EOF
#include <stdio.h>
#include <string.h>
#include <verifier.h>
int main(void)
{
    char buf[8];
    strcpy(buf, "abc");
    verifier_assert(sizeof(long) == 4 && sizeof(void *) == 4);
    return (int)strlen(buf);
}
EOF
clang -m32 -g -S -emit-llvm -Xclang -disable-O0-optnone -fno-discard-value-names t.c -o t.raw.ll
opt -S -p=mem2reg t.raw.ll -o t.ll
grep -q "^target triple = \"i386-" t.ll
svf-verifier -property=reach t.ll
' > "$report"
    jq -e '.status == "ok" and .properties.reach == "proved"' "$report" > /dev/null \
        || { jq . "$report"; echo "ILP32 check failed" >&2; exit 1; }
    echo "ILP32 check passed"
else
    echo "ILP32 check skipped: $arch image has no i386 headers"
fi

expect_exit() {
    local expected="$1"
    shift
    local status=0
    "${run[@]}" "$image" "$@" > /dev/null 2>&1 || status=$?
    if [ "$status" -ne "$expected" ]; then
        echo "expected exit $expected, got $status: $*" >&2
        exit 1
    fi
}

expect_exit 2 svf-verifier -property=bogus /dev/null
expect_exit 1 svf-verifier -property=reach /work/missing.ll

echo "smoke test passed: $image"
