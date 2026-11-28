// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "window_placement.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "data_dir.h"

namespace blocktime {

std::optional<WindowPlacement> loadWindowPlacement(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return std::nullopt;
    const nlohmann::json j = nlohmann::json::parse(in, nullptr, /*allow_exceptions=*/false);
    if (!j.is_object()) return std::nullopt;
    const auto integer = [&j](const char* key) { return j.contains(key) && j[key].is_number_integer(); };
    if (!j.contains("maximized") || !j["maximized"].is_boolean() || !integer("left") || !integer("top") ||
        !integer("right") || !integer("bottom")) {
        return std::nullopt;
    }
    WindowPlacement p;
    p.maximized = j["maximized"].get<bool>();
    p.left = j["left"].get<int>();
    p.top = j["top"].get<int>();
    p.right = j["right"].get<int>();
    p.bottom = j["bottom"].get<int>();
    if (p.right <= p.left || p.bottom <= p.top) return std::nullopt;
    return p;
}

bool saveWindowPlacement(const std::filesystem::path& file, const WindowPlacement& placement) {
    const nlohmann::json j = {{"maximized", placement.maximized},
                              {"left", placement.left},
                              {"top", placement.top},
                              {"right", placement.right},
                              {"bottom", placement.bottom}};
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << j.dump(2) << '\n';
    return static_cast<bool>(out);
}

bool reopenMaximized(bool minimized, bool maximized, bool restoresToMaximized) {
    return minimized ? restoresToMaximized : maximized;
}

std::filesystem::path defaultWindowPlacementFile() { return dataDir() / "window.json"; }

}  // namespace blocktime
