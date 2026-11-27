// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// blocktime-airframe (docs/fleet.md): sends a screenshot of a SimBrief airframe page to Claude, as Add Aircraft does,
// and prints what was read, so the reading can be checked against real screenshots without the app. The key comes
// from settings.json (claude_api_key) and is never printed.

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

#include <QCoreApplication>
#include <QStringList>
#include <nlohmann/json.hpp>

#include "airframe_read.h"
#include "app_settings.h"
#include "claude_reviewer.h"
#include "fleet_rules.h"

using namespace blocktime;

namespace {

std::string show(const std::string& value) { return value.empty() ? "(not read)" : value; }
std::string show(const std::optional<int>& value) { return value ? std::to_string(*value) : "(not read)"; }

void print(const SimbriefReading& r) {
    std::cout << "Civil Registration:   " << show(r.registration) << "\n"
              << "Fin Number:           " << show(r.finNumber) << "\n"
              << "Internal ID:          " << show(r.internalId) << "\n"
              << "Base Type:            " << show(r.baseType) << "\n"
              << "Weight Units:         " << show(r.weightUnits) << "\n"
              << "Max Passengers:       " << show(r.maxPassengers) << "\n"
              << "Passenger Weight:     " << show(r.passengerWeightLb) << "\n"
              << "Baggage Weight:       " << show(r.bagWeightLb) << "\n"
              << "Empty Weight:         " << show(r.emptyWeightLb) << "\n"
              << "Max Zero Fuel Weight: " << show(r.mzfwLb) << "\n"
              << "Max Takeoff Weight:   " << show(r.mtowLb) << "\n"
              << "Max Landing Weight:   " << show(r.mlwLb) << "\n"
              << "Max Fuel Capacity:    " << show(r.maxFuelLb) << "\n"
              << "Max Cargo Weight:     " << show(r.maxCargoLb) << "\n"
              << "Min Runway:           " << show(r.minRunwayFt) << "\n";
    for (const std::string& m : missingFields(r)) std::cout << "Missing: " << m << "\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    QCoreApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();
    if (args.size() != 2) {
        std::cerr << "Usage: blocktime-airframe <screenshot.png|.jpg>\n";
        return 2;
    }
    const std::filesystem::path file = args[1].toStdWString();
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        std::cerr << "Can't read " << file.string() << "\n";
        return 1;
    }
    std::stringstream bytes;
    bytes << in.rdbuf();
    std::string ext = file.extension().string();
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    const std::string mediaType = ext == ".png" ? "image/png" : "image/jpeg";

    const AppSettings settings = loadAppSettings(defaultSettingsFile());
    ClaudeReviewer claude(settings.claudeApiKey);
    if (!claude.hasKey()) {
        std::cerr << "No claude_api_key in " << defaultSettingsFile().string() << "\n";
        return 1;
    }
    int exitCode = 1;
    claude.write(buildAirframeReadRequest(bytes.str(), mediaType), [&](int status, QByteArray answer, QString) {
        const AirframeReadAnswer a = status == 0 ? AirframeReadAnswer{std::nullopt, "Couldn't reach Claude"}
                                                 : parseAirframeReadResponse(status, answer.toStdString());
        if (a.reading) {
            print(*a.reading);
            exitCode = 0;
        } else {
            std::cout << a.error << "\n";
        }
        const auto j = nlohmann::json::parse(answer.toStdString(), nullptr, false);
        if (j.is_object() && j.contains("usage")) std::cout << "\nusage: " << j["usage"].dump() << "\n";
        QCoreApplication::quit();
    });
    app.exec();
    return exitCode;
}
