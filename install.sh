#!/usr/bin/env bash
#
# Build a Release and install it under ~/.local.
#
#     ./install.sh                 into ~/.local
#     ./install.sh /some/prefix    somewhere else
#
# No root is needed, and ~/.local/bin is already on the PATH the Plasma shell
# sees. Three things are installed: the showfavicon binary, the plasmoid package
# and the icon. ShowFavicon has no daemon, so there is no systemd unit and no
# D-Bus service to go with it.
#
# Plasma compiles an applet's QML once and keeps it, so the panel does not show a
# changed widget until the shell reads the package again:
#
#     scripts/reload-plasmoid.sh
#
# To undo it, the VS Code task "Install: local (uninstall)" removes exactly what
# cmake recorded in build-local/install_manifest.txt.

set -euo pipefail

case "${1:-}" in
    -h|--help)
        printf 'usage: %s [install-prefix]\n' "${0##*/}"
        printf '       default prefix: %s\n' "$HOME/.local"
        exit 0
        ;;
esac

here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

prefix="${1:-$HOME/.local}"
build_dir="build-local"

for tool in cmake ninja; do
    command -v "$tool" >/dev/null 2>&1 || { printf '%s is not on the PATH\n' "$tool" >&2; exit 1; }
done

# The install rules bake the prefix in at configure time, so it is passed here
# rather than to `cmake --install`: reconfiguring is cheap, and it keeps the cache
# from disagreeing with what is actually installed.
cd "$here"

printf 'configuring Release with prefix %s\n\n' "$prefix"

cmake -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$prefix" -DBUILD_TESTING=OFF
cmake --build "$build_dir"
cmake --install "$build_dir"

printf '\ninstalled\n'
printf '  %s/bin/showfavicon\n' "$prefix"
printf '  %s/share/plasma/plasmoids/com.martin.showfavicon\n' "$prefix"

if [ -x "$prefix/bin/showfavicon" ]; then
    printf '\n  %s/bin/showfavicon --version → %s\n' "$prefix" "$("$prefix/bin/showfavicon" --version)"
fi

printf '\nnext\n'
printf '  add it    right-click the panel -> Add Widgets -> Show Favicon\n'
printf '            a new widget starts with an empty list; add the websites in its settings\n'
printf '  after a change\n'
printf '            %s/scripts/reload-plasmoid.sh\n' "$here"
