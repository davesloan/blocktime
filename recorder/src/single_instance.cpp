// BlockTime Recorder
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "single_instance.h"

#include <windows.h>

namespace blocktime {

SingleInstance::SingleInstance() {
    handle_ = CreateMutexW(nullptr, FALSE, L"Local\\BlockTimeRecorder");
    // As before (D-037): only an existing mutex blocks. If the mutex can't be created at all, record anyway.
    acquired_ = !(handle_ != nullptr && GetLastError() == ERROR_ALREADY_EXISTS);
}

SingleInstance::~SingleInstance() {
    if (handle_ != nullptr) CloseHandle(handle_);
}

}  // namespace blocktime
