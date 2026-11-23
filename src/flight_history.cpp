// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "flight_history.h"

#include <algorithm>
#include <fstream>
#include <optional>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include "event_log.h"
#include "geo.h"

namespace blocktime {

namespace {

constexpr double kMetersPerNm = 1852.0;

bool hasType(const std::string& line, std::string_view type) {
    return line.find(std::string(R"("type":")") + std::string(type) + '"') != std::string::npos;
}

std::optional<double> number(const nlohmann::json& obj, const char* key) {
    const auto it = obj.find(key);
    if (it == obj.end() || !it->is_number()) return std::nullopt;
    return it->get<double>();
}

std::string text(const nlohmann::json& obj, const char* key) {
    const auto it = obj.find(key);
    if (it == obj.end() || !it->is_string()) return {};
    return it->get<std::string>();
}

// One flight while it's being read: the record plus what's needed to measure and thin its track.
struct OpenFlight {
    FlightRecord rec;
    std::optional<TrackPoint> lastSample;  // for distance, over every sample

    // `force` keeps the point however close it is to the last kept one (the block in position).
    void addPoint(const TrackPoint& p, bool force) {
        if (lastSample) rec.distanceNm += distanceMeters(lastSample->lat, lastSample->lon, p.lat, p.lon) / kMetersPerNm;
        lastSample = p;
        if (rec.track.empty() || force ||
            distanceMeters(rec.track.back().lat, rec.track.back().lon, p.lat, p.lon) >= kTrackMinSpacingM) {
            rec.track.push_back(p);
        }
    }

    // Evenly thins the track to at most kMaxTrackPoints, keeping the first and last point.
    FlightRecord finish() {
        auto& t = rec.track;
        if (t.size() > kMaxTrackPoints) {
            // Every k-th point, plus the last: at most (kMaxTrackPoints - 1) + 1 points.
            const std::size_t k = (t.size() - 1 + kMaxTrackPoints - 3) / (kMaxTrackPoints - 2);
            std::vector<TrackPoint> thinned;
            for (std::size_t i = 0; i < t.size(); i += k) thinned.push_back(t[i]);
            if ((t.size() - 1) % k != 0) thinned.push_back(t.back());
            t = std::move(thinned);
        }
        return std::move(rec);
    }
};

std::optional<TrackPoint> position(const nlohmann::json& data) {
    const auto lat = number(data, "lat");
    const auto lon = number(data, "lon");
    if (!lat || !lon) return std::nullopt;
    return TrackPoint{*lat, *lon, number(data, "alt_ft").value_or(0)};
}

}  // namespace

std::vector<FlightRecord> loadFlightHistory(const std::filesystem::path& dir, std::size_t maxFiles,
                                            std::stop_token stop) {
    std::vector<std::filesystem::path> files = dailyLogFiles(dir);
    if (files.size() > maxFiles) files.erase(files.begin(), files.end() - static_cast<std::ptrdiff_t>(maxFiles));

    std::vector<FlightRecord> flights;  // oldest first while reading
    std::optional<OpenFlight> open;

    for (const auto& file : files) {
        std::ifstream in(file, std::ios::binary);
        std::string line;
        while (std::getline(in, line)) {
            if (stop.stop_requested()) return {};
            // Cheap filter before parsing: most lines are telemetry, and those only matter inside a flight.
            const bool telemetry = hasType(line, "telemetry");
            if (telemetry ? !open
                          : !(hasType(line, "block_out") || hasType(line, "block_in") ||
                              hasType(line, "flight_interrupted"))) {
                continue;
            }
            const auto j = nlohmann::json::parse(line, nullptr, false);
            if (j.is_discarded() || !j.is_object()) continue;
            const auto dataIt = j.find("data");
            if (dataIt == j.end() || !dataIt->is_object()) continue;
            const auto& data = *dataIt;
            const std::string type = text(j, "type");
            const std::string ts = text(j, "ts");

            if (type == "telemetry" && open) {
                if (const auto p = position(data)) {
                    open->rec.maxAltFt = std::max(open->rec.maxAltFt, p->altFt);
                    open->addPoint(*p, false);
                }
            } else if (type == "block_out") {
                if (open) {
                    open->rec.end = FlightEnd::Interrupted;
                    open->rec.endReason = "unclosed";
                    open->rec.endTime = ts;
                    flights.push_back(open->finish());
                }
                open.emplace();
                auto& r = open->rec;
                r.flightId = text(data, "flight_id");
                r.atcId = text(data, "atc_id");
                r.title = text(data, "title");
                r.outTime = text(data, "time");
                if (r.outTime.empty()) r.outTime = ts;
                r.outFuelLb = number(data, "fuel_lb").value_or(0);
                if (auto p = position(data)) {
                    p->altFt = 0;
                    open->addPoint(*p, true);
                }
            } else if (open && text(data, "flight_id") == open->rec.flightId) {
                auto& r = open->rec;
                if (type == "block_in") {
                    r.end = FlightEnd::BlockIn;
                    r.endTime = text(data, "time");
                    if (r.endTime.empty()) r.endTime = ts;
                    r.inFuelLb = number(data, "fuel_lb");
                    if (const auto s = number(data, "block_time_s")) r.blockTimeS = static_cast<int>(*s);
                    if (auto p = position(data)) {
                        p->altFt = 0;
                        open->addPoint(*p, true);
                    }
                } else {
                    r.end = FlightEnd::Interrupted;
                    r.endReason = text(data, "reason");
                    r.endTime = ts;
                }
                flights.push_back(open->finish());
                open.reset();
            }
        }
    }
    if (open) flights.push_back(open->finish());

    std::reverse(flights.begin(), flights.end());
    return flights;
}

}  // namespace blocktime
