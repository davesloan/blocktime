// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "atc_message.h"

namespace blocktime {

namespace {

// A string field, or "" when it's missing or SayIntentions sent null (or a number).
std::string text(const nlohmann::json& e, const char* key) {
    const auto it = e.find(key);
    return it != e.end() && it->is_string() ? it->get<std::string>() : std::string();
}

}  // namespace

std::optional<AtcMessage> atcMessage(const nlohmann::json& atcExchangeData) {
    if (!atcExchangeData.is_object()) return std::nullopt;
    const auto it = atcExchangeData.find("entry");
    if (it == atcExchangeData.end() || !it->is_object()) return std::nullopt;
    const nlohmann::json& e = *it;
    if (text(e, "channel").rfind("COM", 0) != 0) return std::nullopt;
    AtcMessage m;
    if (const auto id = e.find("id"); id != e.end() && id->is_number_integer()) m.id = id->get<long long>();
    m.time = text(e, "stamp_zulu");
    m.airport = text(e, "ident");
    m.position = text(e, "position");
    m.station = text(e, "station_name");
    m.frequency = text(e, "frequency");
    m.atc = text(e, "outgoing_message");
    m.pilot = text(e, "incoming_message");
    return m;
}

}  // namespace blocktime
