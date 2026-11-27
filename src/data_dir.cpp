// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "data_dir.h"

#include <cstdlib>
#include <optional>

#include "sqlite_db.h"

namespace blocktime {

namespace {

std::optional<std::filesystem::path>& chosen() {
    static std::optional<std::filesystem::path> dir;
    return dir;
}

}  // namespace

std::filesystem::path realDataDir() {
    wchar_t* base = nullptr;
    std::size_t len = 0;
    std::filesystem::path root;
    if (_wdupenv_s(&base, &len, L"LOCALAPPDATA") == 0 && base != nullptr) {
        root = base;
        std::free(base);
    } else {
        root = std::filesystem::current_path();
    }
    return root / L"BlockTime";
}

std::filesystem::path dataDir() { return chosen() ? *chosen() : realDataDir(); }

void setDataDir(std::filesystem::path dir) { chosen() = std::move(dir); }

bool seedDataDir(const std::filesystem::path& from, const std::filesystem::path& to, bool withEvents,
                 std::string* error) {
    const auto fail = [error](const std::string& message) {
        if (error) *error = message;
        return false;
    };
    std::error_code ec;
    std::filesystem::create_directories(to, ec);
    if (ec) return fail("can't create " + to.string() + ": " + ec.message());

    if (const auto db = from / "blocktime.db"; std::filesystem::exists(db, ec)) {
        std::string message;
        auto source = db::Database::openReadOnly(db, &message);
        if (!source) return fail("can't open " + db.string() + ": " + message);
        db::Statement vacuum(*source, "VACUUM INTO ?");
        const std::u8string name = (to / "blocktime.db").u8string();  // sqlite3 takes UTF-8
        vacuum.bind(1, std::string_view(reinterpret_cast<const char*>(name.data()), name.size()));
        if (!vacuum.run()) return fail("can't copy blocktime.db: " + vacuum.error());
    }
    for (std::filesystem::directory_iterator it(from, ec), end; !ec && it != end; it.increment(ec)) {
        const auto& p = it->path();
        std::error_code typeEc;
        if (!it->is_regular_file(typeEc)) continue;
        if (p.extension() != ".yaml" && p.filename() != "settings.json") continue;
        std::error_code copyEc;
        std::filesystem::copy_file(p, to / p.filename(), std::filesystem::copy_options::overwrite_existing, copyEc);
        if (copyEc) return fail("can't copy " + p.filename().string() + ": " + copyEc.message());
    }
    if (withEvents && std::filesystem::exists(from / "events", ec)) {
        std::error_code copyEc;
        std::filesystem::copy(from / "events", to / "events",
                              std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
                              copyEc);
        if (copyEc) return fail("can't copy the event logs: " + copyEc.message());
    }
    if (error) error->clear();
    return true;
}

}  // namespace blocktime
