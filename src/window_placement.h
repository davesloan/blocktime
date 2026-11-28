// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Where the main window was when it last closed (D-249): dataDir()\window.json. The app hands the rectangle to
// Windows' SetWindowPlacement, which knows about several monitors, their scaling and monitors that have gone, so this
// file only stores what GetWindowPlacement gave. No windows.h here.

#pragma once

#include <filesystem>
#include <optional>

namespace blocktime {

struct WindowPlacement {
    bool maximized = false;
    // The window's normal (not maximized) rectangle, in Windows' workspace coordinates: what rcNormalPosition holds.
    int left = 0, top = 0, right = 0, bottom = 0;
};

// Nothing for a missing or unreadable file, or a rectangle with no width or height: the window opens as it always has.
std::optional<WindowPlacement> loadWindowPlacement(const std::filesystem::path& file);
bool saveWindowPlacement(const std::filesystem::path& file, const WindowPlacement& placement);

// Whether a window closed in this state reopens maximized. A minimized window comes back as it would have restored.
bool reopenMaximized(bool minimized, bool maximized, bool restoresToMaximized);

std::filesystem::path defaultWindowPlacementFile();  // dataDir()\window.json

}  // namespace blocktime
