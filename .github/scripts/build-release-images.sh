#!/usr/bin/env bash
# Build the release image for each platform, smoke-test it, check that all
# platforms built the same SVF and LLVM, and print the release label.
#
# Usage: build-release-images.sh <local image> <SVF commit> <version> <verifier commit> <platform>...
# Images are tagged <local image>:<arch>. The last three output lines are
# LABEL=..., BUILT_SVF_COMMIT=... and BUILT_LLVM_VERSION=..., suitable for $GITHUB_ENV.
set -euo pipefail

local_image="$1"
svf_sha="$2"
version="$3"
verifier_commit="$4"
shift 4
platforms=("$@")
if [ "${#platforms[@]}" -eq 0 ]; then
    echo "no platforms given" >&2
    exit 2
fi
script_dir="$(cd "$(dirname "$0")" && pwd)"

for platform in "${platforms[@]}"; do
    arch="${platform#linux/}"
    echo "::group::build $platform" >&2
    # The build stage runs ctest; a failing test fails the build.
    podman build --platform "$platform" \
        --build-arg "SVF_REF=$svf_sha" \
        --build-arg "VERIFIER_GIT_COMMIT=$verifier_commit" \
        --build-arg "VERIFIER_VERSION=$version" \
        -t "$local_image:$arch" . >&2
    echo "::endgroup::" >&2
    echo "::group::smoke test $platform" >&2
    bash "$script_dir/smoke-test.sh" "$local_image:$arch" "$platform" "$version" >&2
    echo "::endgroup::" >&2
done

svf="" llvm=""
for platform in "${platforms[@]}"; do
    image="$local_image:${platform#linux/}"
    s="$(podman run --rm --platform "$platform" "$image" cat /opt/svf/SVF_COMMIT)"
    l="$(podman run --rm --platform "$platform" "$image" cat /opt/svf/LLVM_VERSION)"
    if [ -n "$svf" ] && { [ "$s" != "$svf" ] || [ "$l" != "$llvm" ]; }; then
        echo "platforms differ: SVF $svf/$s, LLVM $llvm/$l" >&2
        exit 1
    fi
    svf="$s" llvm="$l"
done
label="$version-svf-${svf:0:7}-llvm-${llvm%%.*}"
if git rev-parse -q --verify "refs/tags/v$label" > /dev/null; then
    echo "tag v$label already exists" >&2
    exit 1
fi
echo "release label: $label" >&2
echo "LABEL=$label"
echo "BUILT_SVF_COMMIT=$svf"
echo "BUILT_LLVM_VERSION=$llvm"
