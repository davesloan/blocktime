// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// blocktime-debrief: the debrief of one flight from a saved event log (docs/debrief.md section 8), with the same code
// the app uses, so the checks, the scoring and Claude's request can be tuned on real flights without flying.
//
//   blocktime-debrief --log-dir <folder> --flight <flight_id> [--diverted] [--prompt] [--claude]
//
// --prompt prints the request Claude would get (no call, no cost). --claude sends it with claude_api_key from
// settings.json and prints the review (it costs money). The tool reads no database: no earlier flights' misses.

#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <format>
#include <iostream>
#include <string>

#include <QCoreApplication>
#include <QStringList>

#include <nlohmann/json.hpp>

#include "app_settings.h"
#include "claude_reviewer.h"
#include "debrief_measure.h"
#include "debrief_review.h"
#include "debrief_score.h"
#include "event_log.h"
#include "flight_events.h"

using namespace blocktime;

namespace {

constexpr std::size_t kAllFiles = 1000;  // every daily log in the folder

std::string header(const std::vector<LogLine>& lines, const std::string& id) {
    std::string tail, title, out, in;
    for (const LogLine& l : lines) {
        if (l.type == "block_out") {
            tail = l.data.value("atc_id", "");
            title = l.data.value("title", "");
            out = l.data.value("time", "");
        }
        if (l.type == "block_in") in = l.data.value("time", "");
    }
    const auto hhmm = [](const std::string& t) { return t.size() >= 16 ? t.substr(11, 5) + "Z" : t; };
    return std::format("{} ({}) · flight {} · out {} in {}", tail, title, id, hhmm(out), hhmm(in));
}

void printDebrief(const FlightMeasurements& m) {
    const ScoredFlight s = scoreFlight(m);
    for (const CheckOutcome& c : s.checks) {
        const char* mark = c.state == CheckState::Passed ? "✓" : c.state == CheckState::Missed ? "✗" : "–";
        std::cout << std::format("{} {:<36} {:<44} {}\n", mark, c.label, c.detail,
                                 c.points < 0 ? std::format("−{}", -c.points) : "");
    }
    if (const auto& l = m.landing) {
        std::cout << std::format("\nLanding: {} fpm, {}.{:02} G, {} bounce(s), {} kt", l->vsFpm, l->gHundredths / 100,
                                 l->gHundredths % 100, l->bounces, l->iasKt);
        if (l->vrefKt) std::cout << std::format(" (VREF {})", *l->vrefKt);
        std::cout << std::format(", wind {:03}/{} ({} kt cross)  {}  {}\n", l->windDirDeg, l->windKt,
                                 s.crosswindKt.value_or(0), starsText(*s.stars),
                                 s.landingPoints < 0 ? std::format("−{}", -s.landingPoints) : "");
    } else {
        std::cout << "\nLanding: no touchdown logged\n";
    }
    std::cout << std::format("\nScore {}{}\n", s.score, s.stars ? " · Landing " + starsText(*s.stars) : "");
}

}  // namespace

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    QCoreApplication app(argc, argv);
    std::filesystem::path logDir;
    std::string flight;
    bool diverted = false, prompt = false, claude = false;
    const QStringList args = QCoreApplication::arguments();
    for (qsizetype i = 1; i < args.size(); ++i) {
        if (args[i] == "--log-dir" && i + 1 < args.size()) logDir = args[++i].toStdWString();
        else if (args[i] == "--flight" && i + 1 < args.size()) flight = args[++i].toStdString();
        else if (args[i] == "--diverted") diverted = true;
        else if (args[i] == "--prompt") prompt = true;
        else if (args[i] == "--claude") claude = true;
    }
    if (logDir.empty() || flight.empty()) {
        std::cerr << "Usage: blocktime-debrief --log-dir <folder> --flight <flight_id> [--diverted] [--prompt] "
                     "[--claude]\n";
        return 2;
    }
    const std::vector<LogLine> lines = loadFlightEvents(logDir, flight, kAllFiles);
    if (lines.empty()) {
        std::cerr << "Flight " << flight << " isn't in " << logDir.string() << " (or has no block_in).\n";
        return 1;
    }
    ReviewInput in;
    in.header = header(lines, flight);
    in.events = lines;
    in.measurements = measureFlight(lines, diverted);
    printDebrief(in.measurements);
    if (!prompt && !claude) return 0;

    const std::string body = buildReviewRequest(in);
    if (prompt) {
        const auto j = nlohmann::json::parse(body);
        std::cout << "\n--- system ---\n" << j["system"].get<std::string>() << "\n\n--- user ---\n"
                  << j["messages"][0]["content"].get<std::string>() << "\n";
    }
    if (!claude) return 0;

    const AppSettings settings = loadAppSettings(defaultSettingsFile());
    ClaudeReviewer reviewer(settings.claudeApiKey);
    if (!reviewer.hasKey()) {
        std::cerr << "No claude_api_key in " << defaultSettingsFile().string() << "\n";
        return 1;
    }
    int exitCode = 1;
    reviewer.write(body, [&](int status, QByteArray answer, QString) {
        const ReviewAnswer a = status == 0 ? ReviewAnswer{std::nullopt, "Couldn't reach Claude"}
                                           : parseReviewResponse(status, answer.toStdString());
        if (a.review) {
            std::cout << "\n--- review (" << a.review->model << ") ---\n" << a.review->summary << "\n\nTo work on:"
                      << (a.review->workOn.empty() ? " nothing\n" : "\n");
            for (const std::string& w : a.review->workOn) std::cout << "- " << w << "\n";
            std::cout << "\nLanding: " << a.review->landing << "\n\nATC: " << a.review->atc << "\n";
            exitCode = 0;
        } else {
            std::cout << "\n" << a.error << "\n";
        }
        const auto j = nlohmann::json::parse(answer.toStdString(), nullptr, false);
        if (j.is_object() && j.contains("usage")) std::cout << "\nusage: " << j["usage"].dump() << "\n";
        QCoreApplication::quit();
    });
    app.exec();
    return exitCode;
}
