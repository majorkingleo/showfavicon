#!/usr/bin/env bash
#
# Configure and build ShowFavicon.
#
#     ./build.sh                     Debug, into ./build
#     ./build.sh Release             Release, into ./build-release
#     ./build.sh Release my-dir      an explicit directory
#
# The VS Code task "CMake: build (Debug)" does the same thing; this is the version
# that needs nothing but a shell. It only builds: ./install.sh puts the result
# into ~/.local, and the suite runs with
#
#     ctest --test-dir build --output-on-failure

set -euo pipefail

case "${1:-}" in
    -h|--help)
        printf 'usage: %s [Debug|Release|RelWithDebInfo|MinSizeRel] [build-dir]\n' "${0##*/}"
        exit 0
        ;;
esac

here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

build_type="${1:-Debug}"
build_dir="${2:-}"

case "$build_type" in
    Debug|Release|RelWithDebInfo|MinSizeRel) ;;
    *)
        printf 'unknown build type: %s\n' "$build_type" >&2
        printf 'usage: %s [Debug|Release|RelWithDebInfo|MinSizeRel] [build-dir]\n' "${0##*/}" >&2
        exit 2
        ;;
esac

# One directory per build type, so a Debug build is never silently reconfigured
# into a Release one. An explicit second argument wins.
if [ -z "$build_dir" ]; then
    if [ "$build_type" = "Debug" ]; then
        build_dir="build"
    else
        build_dir="build-$(printf '%s' "$build_type" | tr '[:upper:]' '[:lower:]')"
    fi
fi

for tool in cmake ninja; do
    command -v "$tool" >/dev/null 2>&1 || { printf '%s is not on the PATH\n' "$tool" >&2; exit 1; }
done

cd "$here"

printf 'building %s into %s\n\n' "$build_type" "$build_dir"

cmake -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE="$build_type"
cmake --build "$build_dir"

printf '\nbuilt   %s/src/showfavicon\n' "$build_dir"
printf 'try     %s/src/showfavicon https://example.com\n' "$build_dir"
printf 'tests   ctest --test-dir %s --output-on-failure\n' "$build_dir"
printf 'install ./install.sh\n'
