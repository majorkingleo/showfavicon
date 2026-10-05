#pragma once

#include <QStringList>

namespace ShowFavicon {

/// Runs the command line and returns the process exit code.
///
/// `arguments` is everything after the program name. The exit code is the only
/// channel the widget cannot see; everything that matters goes to stdout as one
/// JSON line.
int runCli(const QStringList &arguments);

} // namespace ShowFavicon
