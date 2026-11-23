// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Only one recorder at a time; a second copy of the app would write every event twice (D-037).

#pragma once

namespace blocktime {

class SingleInstance {
public:
    // Creates or opens Local\BlockTimeRecorder and holds it for the life of the object.
    SingleInstance();
    ~SingleInstance();
    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    // False if another recorder already holds the mutex.
    bool acquired() const { return acquired_; }

private:
    void* handle_ = nullptr;  // HANDLE (void* keeps windows.h out of this header)
    bool acquired_ = true;
};

}  // namespace blocktime
