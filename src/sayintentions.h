// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SayIntentions' getWX (docs/notes/sayintentions-api.md): the active runways for New Flight's planning (D-105). The
// app sends the request, with the developer key in a header; this builds the URL and reads the answer loosely,
// since the live fields differ from the documented ones.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace blocktime {

// The developer key isn't part of the URL: it goes in the X-Developer-Key header.
std::string getWxUrl(std::string_view apiKey, std::string_view origin, std::string_view destination);

struct AtisRunways {
    std::string origin, destination;  // "" = none found; SimBrief chooses
};

// The origin's first departing runway and the destination's first arriving one, each falling back to
// active_runway. Anything unreadable gives "".
AtisRunways parseAtisRunways(std::string_view json, std::string_view origin, std::string_view destination);

// The text of an {"error": "..."} answer (a wrong key, a server error), or "" when there's none.
std::string atisError(std::string_view json);

// "9R" → "09R"; "" for anything that isn't 1-2 digits and an optional L, R or C.
std::string normalizeRunway(std::string_view runway);

// ---- During the flight (D-123): the ATIS and the ATC log, for milestone 10's analysis ----

std::string getWxUrl(std::string_view apiKey, std::string_view icao);  // one airport
std::string commsHistoryUrl(std::string_view apiKey);                  // getCommsHistory, no since_id (D-127)

// One airport's ATIS from getWX's live fields (letter is "current", text is "atis"). Numbers come back as text.
struct Atis {
    std::string icao, letter, text, metar, altimeter, departing, arriving;
};
// nullopt for an error, an answer that isn't getWX's, or an airport not in it.
std::optional<Atis> parseAtis(std::string_view json, std::string_view icao);

// getCommsHistory's answer: SayIntentions' flight id and each exchange as it came (logged verbatim).
struct CommsHistory {
    std::string flightId;
    std::vector<nlohmann::json> entries;
};
std::optional<CommsHistory> parseCommsHistory(std::string_view json);

}  // namespace blocktime
