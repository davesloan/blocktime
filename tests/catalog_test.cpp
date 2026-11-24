// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string>

#include <gtest/gtest.h>

#include "catalog.h"

using namespace blocktime;

namespace {

const std::string kType = R"(types:
  - code: 777F
    family: "777"
    role: freighter
    turnaround_min: 60
    cruise_speed_kt: 490
    lease_per_month: 3000000
    sim_folder: PMDG 777F
)";

std::string withAirframes(const std::string& airframes) { return kType + "airframes:\n" + airframes; }

std::string parseError(const std::string& yaml) {
    std::string error;
    EXPECT_FALSE(parseCatalog(yaml, &error).has_value());
    return error;
}

}  // namespace

TEST(Catalog, ParsesTheRepoCatalog) {
    std::string error;
    const auto catalog = loadCatalog(BLOCKTIME_SOURCE_DIR "/config/catalog.yaml", &error);
    ASSERT_TRUE(catalog.has_value()) << error;
    const CatalogType* bbj = catalog->type("737-800 BBJ2");
    ASSERT_NE(bbj, nullptr);
    EXPECT_EQ(bbj->family, "737");
    EXPECT_EQ(bbj->role, "executive");
    EXPECT_EQ(bbj->turnaroundMin, 35);
    EXPECT_EQ(bbj->monthlyLeaseCents, 15'000'000);

    const CatalogAirframe* vip = catalog->airframe("737-800 BBJ2 VIP");
    ASSERT_NE(vip, nullptr);
    EXPECT_EQ(vip->typeCode, "737-800 BBJ2");
    ASSERT_EQ(vip->cabins.size(), 1u);
    EXPECT_EQ(vip->cabins[0].cabinClass, "economy");
    EXPECT_EQ(vip->cabins[0].seats, 48);
    ASSERT_EQ(vip->holds.size(), 2u);
    EXPECT_EQ(vip->holds[0].hold, "forward");
    EXPECT_EQ(vip->holds[0].maxLb, 7842);
    EXPECT_EQ(vip->holds[0].ratio, 4);

    EXPECT_EQ(catalog->types.size(), 7u);
    EXPECT_EQ(catalog->airframes.size(), 9u);
    EXPECT_EQ(catalog->type("747"), nullptr);
}

TEST(Catalog, AcceptsAFreighterWithHoldsOnly) {
    std::string error;
    const auto catalog = parseCatalog(withAirframes(R"(  - name: 777F Freighter
    type: 777F
    holds:
      - { hold: main_deck, max_lb: 180000, ratio: 40 }
    limits: { min_runway_ft: 10000, mlw_lb: 575000, passenger_weight_lb: 190, bag_weight_lb: 55, max_payload_lb: 228700 }
)"),
                                      &error);
    ASSERT_TRUE(catalog.has_value()) << error;
    EXPECT_TRUE(catalog->airframe("777F Freighter")->cabins.empty());
}

TEST(Catalog, RejectsCabinsOnAFreighter) {
    const std::string error = parseError(withAirframes(R"(  - name: 777F Freighter
    type: 777F
    cabins: { economy: 10 }
    holds:
      - { hold: main_deck, max_lb: 180000, ratio: 40 }
)"));
    EXPECT_NE(error.find("freighter"), std::string::npos) << error;
    EXPECT_NE(error.find("cabins"), std::string::npos) << error;
    EXPECT_EQ(error.rfind("catalog.yaml line ", 0), 0u) << error;
}

TEST(Catalog, RejectsUnknownType) {
    const std::string error = parseError(withAirframes(R"(  - name: Mystery
    type: 747-400
    holds:
      - { hold: forward, max_lb: 1000, ratio: 1 }
)"));
    EXPECT_NE(error.find("unknown type"), std::string::npos) << error;
}

TEST(Catalog, RejectsLeaseNotMultipleOfFive) {
    std::string yaml = withAirframes("  []\n");
    yaml.replace(yaml.find("3000000"), 7, "3000001");
    const std::string error = parseError(yaml);
    EXPECT_NE(error.find("multiple of 5"), std::string::npos) << error;
}

TEST(Catalog, RejectsBadRoleAndDuplicateHold) {
    std::string yaml = withAirframes("  []\n");
    yaml.replace(yaml.find("role: freighter"), 15, "role: bomber");
    EXPECT_NE(parseError(yaml).find("role"), std::string::npos);

    const std::string error = parseError(withAirframes(R"(  - name: Twice
    type: 777F
    holds:
      - { hold: forward, max_lb: 1000, ratio: 1 }
      - { hold: forward, max_lb: 1000, ratio: 1 }
)"));
    EXPECT_NE(error.find("forward"), std::string::npos) << error;
}

TEST(Catalog, AirframeLimitsAreRead) {
    std::string error;
    const auto catalog = loadCatalog(BLOCKTIME_SOURCE_DIR "/config/catalog.yaml", &error);
    ASSERT_TRUE(catalog.has_value()) << error;
    const AirplaneLimits& l = catalog->airframe("737-800 BBJ2 VIP")->limits;
    EXPECT_EQ(l.minRunwayFt, 7000);
    EXPECT_EQ(l.mlwLb, 146300);
    EXPECT_EQ(l.passengerWeightLb, 190);
    EXPECT_EQ(l.bagWeightLb, 55);
    EXPECT_EQ(l.maxPayloadLb, 36113);
}

TEST(Catalog, TypesHaveSimFolders) {
    std::string error;
    const auto catalog = loadCatalog(BLOCKTIME_SOURCE_DIR "/config/catalog.yaml", &error);
    ASSERT_TRUE(catalog.has_value()) << error;
    EXPECT_EQ(catalog->type("737-800 BBJ2")->simFolder, "PMDG 737-800BBJ2");
    EXPECT_EQ(catalog->type("737-800")->simFolder, "PMDG 737-800");
    EXPECT_EQ(catalog->type("777-200ER")->simFolder, "PMDG 777-200ER");
}

TEST(Catalog, SimFolderIsRequired) {
    const std::string line = "    sim_folder: PMDG 777F\n";
    std::string yaml = kType;
    yaml.replace(yaml.find(line), line.size(), "");
    std::string error;
    EXPECT_FALSE(parseCatalog(yaml + "airframes: []\n", &error).has_value());
    EXPECT_NE(error.find("sim_folder"), std::string::npos) << error;
}
