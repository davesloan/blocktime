// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "config_yaml.h"

#include <format>
#include <fstream>
#include <sstream>

namespace blocktime::config {

bool YamlReader::fail(const YAML::Mark& mark, std::string_view message) {
    if (error_ && error_->empty()) {
        *error_ = mark.is_null() ? std::format("{}: {}", file_, message)
                                 : std::format("{} line {}: {}", file_, mark.line + 1, message);
    }
    return false;
}

bool YamlReader::fail(const YAML::Node& at, std::string_view message) {
    return fail(at.IsDefined() ? at.Mark() : YAML::Mark::null_mark(), message);
}

bool YamlReader::string(const YAML::Node& map, const char* key, std::string& out) {
    const YAML::Node node = map[key];
    if (!node.IsDefined() || node.IsNull()) return fail(map, std::format("missing {}", key));
    if (!node.IsScalar()) return fail(node, std::format("{} must be a single value", key));
    out = node.as<std::string>();
    return true;
}

bool YamlReader::integer(const YAML::Node& map, const char* key, std::int64_t& out) {
    std::string text;
    if (!string(map, key, text)) return false;
    try {
        out = map[key].as<std::int64_t>();
    } catch (const YAML::BadConversion&) {
        return fail(map[key], std::format("{} must be a whole number, not \"{}\"", key, text));
    }
    return true;
}

bool YamlReader::optionalString(const YAML::Node& map, const char* key, std::string& out) {
    const YAML::Node node = map[key];
    if (!node.IsDefined() || node.IsNull()) return true;
    return string(map, key, out);
}

bool YamlReader::optionalBool(const YAML::Node& map, const char* key, bool& out) {
    const YAML::Node node = map[key];
    if (!node.IsDefined() || node.IsNull()) return true;
    try {
        out = node.as<bool>();
    } catch (const YAML::BadConversion&) {
        return fail(node, std::format("{} must be true or false", key));
    }
    return true;
}

std::optional<YAML::Node> parseYaml(std::string_view text, YamlReader& reader) {
    try {
        return YAML::Load(std::string(text));
    } catch (const YAML::Exception& e) {
        reader.fail(e.mark, e.msg);
        return std::nullopt;
    }
}

std::optional<std::string> readFile(const std::filesystem::path& file, std::string* error) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        if (error) *error = "can't read " + file.string();
        return std::nullopt;
    }
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

}  // namespace blocktime::config
