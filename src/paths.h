#pragma once

#include <QString>

namespace ShowFavicon::Paths {

/// `~/.local/share/showfavicon`, where the cached icons live.
///
/// Built from GenericDataLocation plus a fixed name rather than from
/// QStandardPaths::AppLocalDataLocation, which appends the organisation and the
/// application name and produced `…/share/ShowFavicon/ShowFavicon` in the project
/// this layout was taken from.
QString dataDir();

/// Turns the path a user typed into an absolute one.
///
/// A relative path is only meaningful next to the directory it was typed in, and
/// that directory is gone the moment the path leaves the process: the widget runs
/// this binary with Plasma's working directory, not the shell's. Existing paths
/// are canonicalised, which also resolves symlinks and `..`; non-existent ones are
/// only made absolute, so an error message names a path the reader can find.
QString resolvePath(const QString &path);

} // namespace ShowFavicon::Paths
