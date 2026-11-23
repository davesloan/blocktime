// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Shared helpers for the YAML config readers (catalog.yaml, airline.yaml; D-095): reading required and optional
// fields, and error messages that name the file and line ("airline.yaml line 12: ...").

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <yaml-cpp/yaml.h>

namespace blocktime::config {

// Collects the first error. Every reader returns false once an error is set.
class YamlReader {
public:
    YamlReader(std::string fileName, std::string* error) : file_(std::move(fileName)), error_(error) {}

    // Sets "<file> line <n>: <message>" (or "<file>: <message>" without a position) and returns false.
    bool fail(const YAML::Node& at, std::string_view message);
    bool fail(const YAML::Mark& mark, std::string_view message);

    // A required key of `map`. Missing, or the wrong kind, is an error at `map`.
    bool string(const YAML::Node& map, const char* key, std::string& out);
    bool integer(const YAML::Node& map, const char* key, std::int64_t& out);
    bool number(const YAML::Node& map, const char* key, double& out);  // whole or decimal
    // Optional keys keep `out` unchanged when missing.
    bool optionalString(const YAML::Node& map, const char* key, std::string& out);
    bool optionalBool(const YAML::Node& map, const char* key, bool& out);

    const std::string& file() const { return file_; }

private:
    std::string file_;
    std::string* error_;
};

// Parses `text`, turning a YAML syntax error into "<file> line <n>: ...".
std::optional<YAML::Node> parseYaml(std::string_view text, YamlReader& reader);

// The whole file as text, or nullopt with "can't read <path>".
std::optional<std::string> readFile(const std::filesystem::path& file, std::string* error);

}  // namespace blocktime::config
