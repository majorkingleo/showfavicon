#!/usr/bin/env sh
#
# Install the fetcher and the plasmoid.
#
# The fetcher goes into ~/.local/bin because that is on the PATH the Plasma
# shell sees; the widget runs it by name. The plasmoid is installed with
# kpackagetool6. A running shell will not show a changed main.qml until it is
# restarted -- use scripts/reload-plasmoid.sh for that.

set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
package="$here/plasmoid/com.martin.showfavicon"

[ -d "$package" ] || { printf 'no such package: %s\n' "$package" >&2; exit 2; }

# 1. The fetcher.
install -d "$HOME/.local/bin"
install -m 755 "$here/tools/showfavicon-fetch" "$HOME/.local/bin/showfavicon-fetch"
printf 'installed %s\n' "$HOME/.local/bin/showfavicon-fetch"

# 2. ImageMagick is optional, but it is what produces the true grayscale copy
#    used for the offline state.
if ! command -v magick >/dev/null 2>&1 && ! command -v convert >/dev/null 2>&1; then
    printf 'warning: ImageMagick not found -- the offline icon will be dimmed instead of grayed\n' >&2
    printf '         install it with: sudo pacman -S imagemagick\n' >&2
fi

# 3. The plasmoid. --upgrade fails when it is not installed yet, which is when
#    --install is the right call.
if ! kpackagetool6 --type Plasma/Applet --upgrade "$package" 2>/dev/null; then
    kpackagetool6 --type Plasma/Applet --install "$package"
fi
printf 'installed plasmoid %s\n' "$package"
printf 'now add it: right-click the panel -> Add Widgets -> "%s"\n' "Show Favicon"
