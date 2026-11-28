// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "window_placement_win.h"

#include <windows.h>

#include <QQuickWindow>

#include "window_placement.h"

void showRememberingPlacement(QQuickWindow& window, const std::filesystem::path& file, bool remember) {
    if (!remember) {
        window.show();
        return;
    }

    // winId() creates the native window without showing it, so it can be placed before it appears (no jump).
    const auto hwnd = reinterpret_cast<HWND>(window.winId());
    bool maximized = false;
    const auto saved = blocktime::loadWindowPlacement(file);
    const RECT savedRect = saved ? RECT{saved->left, saved->top, saved->right, saved->bottom} : RECT{};
    // On no monitor at all (one that's been unplugged): open as the first time, centred on the main monitor, rather
    // than where Windows would pull it back to (measured: straddling two monitors).
    if (saved && MonitorFromRect(&savedRect, MONITOR_DEFAULTTONULL) != nullptr) {
        WINDOWPLACEMENT wp{};
        wp.length = sizeof(wp);
        wp.showCmd = SW_HIDE;  // place it, still hidden; Qt shows it below so it knows the window is visible
        wp.rcNormalPosition = savedRect;
        // Twice: the first move to a monitor with other scaling makes Qt resize the window to keep its QML size; the
        // second, now at that monitor's scaling, sets the saved size (measured on Dave's mixed 4K/1080p monitors).
        SetWindowPlacement(hwnd, &wp);
        SetWindowPlacement(hwnd, &wp);
        maximized = saved->maximized;
    }

    // Saved on every close request, even one Main.qml refuses during a flight (D-229): the window is where it is
    // either way, and the close that goes through saves again.
    QObject::connect(&window, &QQuickWindow::closing, &window, [hwnd, file] {
        WINDOWPLACEMENT wp{};
        wp.length = sizeof(wp);
        if (!GetWindowPlacement(hwnd, &wp)) return;
        const RECT& r = wp.rcNormalPosition;
        blocktime::saveWindowPlacement(
            file, {.maximized = blocktime::reopenMaximized(wp.showCmd == SW_SHOWMINIMIZED, wp.showCmd == SW_SHOWMAXIMIZED,
                                                           (wp.flags & WPF_RESTORETOMAXIMIZED) != 0),
                   .left = r.left,
                   .top = r.top,
                   .right = r.right,
                   .bottom = r.bottom});
    });

    if (maximized) {
        window.showMaximized();
    } else {
        window.show();
    }
}
