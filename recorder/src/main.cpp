// BlockTime Recorder
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Console front end for the recorder. Connects to MSFS, logs telemetry, and detects block out / block in (D-031,
// D-032). Every flight ends with block_in or flight_interrupted; none is resumed (D-046). The loop itself is in
// recorder.cpp, shared with the Qt app (D-053).
// See docs/recorder.md and docs/progress.md.

#include <windows.h>

#include <atomic>
#include <filesystem>
#include <iostream>
#include <string>

#include "event_log.h"
#include "recorder.h"
#include "single_instance.h"

using namespace blocktime;

namespace {

std::atomic<bool> g_stop{false};

BOOL WINAPI onConsoleCtrl(DWORD type) {
    g_stop = true;
    // For window close / logoff / shutdown Windows kills us when this returns; give main a moment to close cleanly.
    if (type == CTRL_CLOSE_EVENT || type == CTRL_LOGOFF_EVENT || type == CTRL_SHUTDOWN_EVENT) Sleep(1500);
    return TRUE;
}

void printUsage() {
    std::cout << "BlockTime Recorder " BLOCKTIME_VERSION "\n"
                 "Usage: blocktime-recorder [--log-dir <folder>]\n"
                 "  --log-dir   where events-YYYY-MM-DD.jsonl files go\n"
                 "              (default: %LOCALAPPDATA%\\BlockTime\\events)\n";
}

class ConsoleObserver : public RecorderObserver {
public:
    void onStarted(const std::filesystem::path& logDir) override {
        std::cout << "BlockTime Recorder " BLOCKTIME_VERSION "\nLogging to " << logDir.string()
                  << "\nPress Ctrl+C to stop.\n";
    }
    void onLine(const std::string& line, bool error) override { (error ? std::cerr : std::cout) << line << "\n"; }
    void onConnection(ConnectionState) override {}
    void onTelemetry(const TelemetrySample& s) override { std::cout << telemetryConsoleLine(s) << "\n"; }
    void onBlockEvent(const BlockEvent&) override {}
};

}  // namespace

int wmain(int argc, wchar_t** argv) {
    std::filesystem::path logDir = EventLog::defaultDir();
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        if (a == L"--log-dir" && i + 1 < argc) {
            logDir = argv[++i];
        } else if (a == L"--help" || a == L"-h" || a == L"/?") {
            printUsage();
            return 0;
        } else {
            std::wcerr << L"Unknown argument: " << a << L"\n";
            printUsage();
            return 2;
        }
    }

    SetConsoleOutputCP(CP_UTF8);
    std::cout << std::unitbuf;  // keep console/redirected output current even if we're killed
    SetConsoleCtrlHandler(onConsoleCtrl, TRUE);

    // Only one recorder at a time (D-037). The mutex is held until the process exits.
    const SingleInstance instance;
    if (!instance.acquired()) {
        std::cerr << "BlockTime Recorder is already running. Close the other window first.\n";
        return 3;
    }

    ConsoleObserver observer;
    return Recorder(logDir, observer).run(g_stop);
}
