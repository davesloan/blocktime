// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "atc_log.h"

namespace blocktime {

namespace {

std::string text(const nlohmann::json& e, const char* name) {
    const auto f = e.find(name);
    if (f == e.end()) return {};
    if (f->is_string()) return f->get<std::string>();
    if (f->is_number()) return f->dump();
    return {};
}

// The entry's id; one without an id is keyed by when, where and what the pilot said.
std::string key(const nlohmann::json& e) {
    const std::string id = text(e, "id");
    if (!id.empty()) return id;
    return text(e, "stamp_zulu") + "|" + text(e, "frequency") + "|" + text(e, "incoming_message");
}

}  // namespace

std::vector<nlohmann::json> CommsDedupe::toLog(const CommsHistory& history) {
    if (history.flightId != flightId_) {
        logged_.clear();
        flightId_ = history.flightId;
    }
    std::vector<nlohmann::json> out;
    for (const nlohmann::json& e : history.entries) {
        std::string content = e.dump();
        std::string& last = logged_[key(e)];
        if (last == content) continue;
        last = std::move(content);
        out.push_back(e);
    }
    return out;
}

void CommsDedupe::reset() {
    flightId_.clear();
    logged_.clear();
}

bool AtisDedupe::isNew(const Atis& atis) {
    std::string content = atis.letter + "\n" + atis.text;
    std::string& last = last_[atis.icao];
    if (last == content) return false;
    last = std::move(content);
    return true;
}

void AtisDedupe::reset() { last_.clear(); }

}  // namespace blocktime
