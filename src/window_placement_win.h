// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The main window reopens where it was last closed (D-249), through Windows' own GetWindowPlacement and
// SetWindowPlacement: they keep the maximized state and the normal rectangle apart, pull a window back onto a monitor
// that's still there, and handle monitors with different scaling. The only file in the app that calls them.

#pragma once

#include <filesystem>

class QQuickWindow;

// Shows `window` (created hidden by Main.qml) where the placement file says, maximized if it was, and saves its
// placement there each time it's asked to close. With `remember` false (a --grab, or not the Windows platform) it just
// shows the window at its QML size.
void showRememberingPlacement(QQuickWindow& window, const std::filesystem::path& file, bool remember);
