// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SayIntentions' getWX (docs/notes/sayintentions-api.md): the active runways for New Flight's planning (D-105). The
// app sends the request, with the developer key in a header; this builds the URL and reads the answer loosely,
// since the live fields differ from the documented ones.

#pragma once

#include <string>
#include <string_view>

namespace blocktime {

// The developer key isn't part of the URL: it goes in the X-Developer-Key header.
std::string getWxUrl(std::string_view apiKey, std::string_view origin, std::string_view destination);

struct AtisRunways {
    std::string origin, destination;  // "" = none found; SimBrief chooses
};

// The origin's first departing runway and the destination's first arriving one, each falling back to
// active_runway. Anything unreadable gives "".
AtisRunways parseAtisRunways(std::string_view json, std::string_view origin, std::string_view destination);

// "9R" → "09R"; "" for anything that isn't 1-2 digits and an optional L, R or C.
std::string normalizeRunway(std::string_view runway);

}  // namespace blocktime
