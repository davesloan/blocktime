// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "catalog.h"

using namespace blocktime;
namespace fs = std::filesystem;

namespace {

const std::string kFreighter = R"(code: 777F
family: "777"
role: freighter
turnaround_min: 60
cruise_speed_kt: 490
lease_per_month: 500000
sim_folder: PMDG 777F
wingspan_m: 64.8
simbrief_base_types: [B77L]
factory: KPAE
delivery_fee_per_nm: 18
layouts:
  - name: 777F Freighter
    holds:
      - { hold: main_deck, share_lb: 180000, ratio: 40 }
)";

const std::string k738 = R"(code: 737-800
family: "737"
role: airliner
turnaround_min: 35
cruise_speed_kt: 450
lease_per_month: 235000
sim_folder: PMDG 737-800
wingspan_m: 35.8
simbrief_base_types: [B738]
factory: KBFI
delivery_fee_per_nm: 8
layouts:
  - name: 737-800 Two-class
    cabins: { business: 16, economy: 144 }
    holds:
      - { hold: forward, share_lb: 7500, ratio: 4 }
      - { hold: aft, share_lb: 9500, ratio: 5 }
  - name: 737-800 All-economy
    cabins: { economy: 184 }
    holds:
      - { hold: forward, share_lb: 7500, ratio: 4 }
      - { hold: aft, share_lb: 9500, ratio: 5 }
)";

std::string parseError(const std::string& yaml) {
    std::string error;
    EXPECT_FALSE(parseAircraftFile(yaml, "test.yaml", &error).has_value());
    return error;
}

std::string replaced(std::string text, const std::string& from, const std::string& to) {
    text.replace(text.find(from), from.size(), to);
    return text;
}

}  // namespace

TEST(AircraftFiles, ShippedFilesAllLoad) {
    std::vector<std::string> notices;
    const Catalog catalog = loadCatalogDir(BLOCKTIME_SOURCE_DIR "/data/aircraft", &notices);
    EXPECT_TRUE(notices.empty()) << (notices.empty() ? "" : notices[0]);
    ASSERT_EQ(catalog.types.size(), 7u);
    EXPECT_EQ(catalog.type("737-600")->simFolder, "PMDG 737-600");
    EXPECT_EQ(catalog.type("737-800")->simFolder, "PMDG 737-800");
    EXPECT_EQ(catalog.type("737-800 BBJ2")->simFolder, "PMDG 737-800BBJ2");
    EXPECT_EQ(catalog.type("737-800 BCF")->simFolder, "PMDG 737-800BCF");
    EXPECT_EQ(catalog.type("777-200ER")->simFolder, "PMDG 777-200ER");
    EXPECT_EQ(catalog.type("777-300ER")->simFolder, "PMDG 777-300ER");
    EXPECT_EQ(catalog.type("777F")->simFolder, "PMDG 777F");
    EXPECT_EQ(catalog.airframes.size(), 9u);

    // N814SX's SimBrief airframe (docs/notes/simbrief-airframes.md): base type BBJ2, 48 seats, 17,645 lb split 4:5.
    const CatalogType* bbj = catalog.type("737-800 BBJ2");
    EXPECT_EQ(bbj->simbriefBaseTypes, std::vector<std::string>{"BBJ2"});
    EXPECT_EQ(bbj->monthlyLeaseCents, 15'000'000);
    const CatalogAirframe* vip = catalog.airframe("737-800 BBJ2 VIP");
    ASSERT_NE(vip, nullptr);
    EXPECT_EQ(vip->cabins[0].seats, 48);
    EXPECT_EQ(vip->holds[0].maxLb, 7842);
    EXPECT_EQ(vip->holds[1].maxLb, 9803);
    EXPECT_EQ(catalog.type("777-300ER")->factoryIcao, "KPAE");
    EXPECT_EQ(catalog.type("777-300ER")->deliveryFeePerNmDollars, 18);
}

TEST(AircraftFiles, ParsesTypeAndLayouts) {
    std::string error;
    const auto catalog = parseAircraftFile(k738, "pmdg-737-800.yaml", &error);
    ASSERT_TRUE(catalog.has_value()) << error;
    ASSERT_EQ(catalog->types.size(), 1u);
    const CatalogType& t = catalog->types[0];
    EXPECT_EQ(t.simbriefBaseTypes, std::vector<std::string>{"B738"});
    EXPECT_EQ(t.factoryIcao, "KBFI");
    EXPECT_EQ(t.deliveryFeePerNmDollars, 8);
    EXPECT_EQ(t.turnaroundMin, 35);
    EXPECT_DOUBLE_EQ(t.wingspanM, 35.8);
    ASSERT_EQ(catalog->airframes.size(), 2u);
    const CatalogAirframe& two = catalog->airframes[0];
    EXPECT_EQ(two.name, "737-800 Two-class");
    EXPECT_EQ(two.typeCode, "737-800");
    ASSERT_EQ(two.holds.size(), 2u);
    EXPECT_EQ(two.holds[0].hold, "forward");
    EXPECT_EQ(two.holds[0].maxLb, 7500);
    EXPECT_EQ(two.holds[0].ratio, 4);
    EXPECT_EQ(two.holds[1].hold, "aft");
    EXPECT_EQ(two.holds[1].maxLb, 9500);
    EXPECT_EQ(two.holds[1].ratio, 5);
}

TEST(AircraftFiles, BadFileIsSkippedWithNotice) {
    const fs::path dir = fs::temp_directory_path() / "blocktime-aircraft-files-test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    std::ofstream(dir / "good.yaml") << k738;
    std::ofstream(dir / "bad.yaml") << "code: [not closed\n";
    std::vector<std::string> notices;
    const Catalog catalog = loadCatalogDir(dir, &notices);
    EXPECT_EQ(catalog.types.size(), 1u);
    ASSERT_EQ(notices.size(), 1u);
    EXPECT_NE(notices[0].find("bad.yaml"), std::string::npos) << notices[0];
    fs::remove_all(dir);
}

TEST(AircraftFiles, AcceptsAFreighterWithHoldsOnly) {
    std::string error;
    const auto catalog = parseAircraftFile(kFreighter, "f.yaml", &error);
    ASSERT_TRUE(catalog.has_value()) << error;
    EXPECT_TRUE(catalog->airframes[0].cabins.empty());
}

TEST(AircraftFiles, RejectsCabinsOnAFreighter) {
    const std::string error = parseError(replaced(kFreighter, "    holds:", "    cabins: { economy: 10 }\n    holds:"));
    EXPECT_NE(error.find("freighter"), std::string::npos) << error;
    EXPECT_EQ(error.rfind("test.yaml line ", 0), 0u) << error;
}

TEST(AircraftFiles, RejectsLeaseNotMultipleOfFive) {
    EXPECT_NE(parseError(replaced(kFreighter, "500000", "500001")).find("multiple of 5"), std::string::npos);
}

TEST(AircraftFiles, RejectsBadRoleAndDuplicateHold) {
    EXPECT_NE(parseError(replaced(kFreighter, "role: freighter", "role: bomber")).find("role"), std::string::npos);
    const std::string twice = replaced(kFreighter, "      - { hold: main_deck, share_lb: 180000, ratio: 40 }",
                                       "      - { hold: forward, share_lb: 1000, ratio: 1 }\n"
                                       "      - { hold: forward, share_lb: 1000, ratio: 1 }");
    EXPECT_NE(parseError(twice).find("forward"), std::string::npos);
}

TEST(AircraftFiles, RequiresSimFolderBaseTypesAndFactory) {
    EXPECT_NE(parseError(replaced(kFreighter, "sim_folder: PMDG 777F\n", "")).find("sim_folder"), std::string::npos);
    EXPECT_NE(parseError(replaced(kFreighter, "simbrief_base_types: [B77L]\n", "")).find("simbrief_base_types"),
              std::string::npos);
    EXPECT_NE(parseError(replaced(kFreighter, "factory: KPAE\n", "")).find("factory"), std::string::npos);
}
