// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <fstream>
#include <iterator>
#include <string>

#include <gtest/gtest.h>

#include "simbrief.h"

using namespace blocktime;

namespace {

std::string fixture(const char* name) {
    std::ifstream in(std::string(BLOCKTIME_SOURCE_DIR "/tests/data/") + name, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

DispatchRequest kbfiKsan() {
    DispatchRequest r;
    r.origin = "KBFI";
    r.destination = "KSAN";
    r.airlineIcao = "XLY";
    r.flightNumber = "101";
    r.airframeId = "730097_1790536204614";
    r.originRunway = "14R";
    r.destinationRunway = "27";
    r.passengers = 46;
    r.cargoLb = 0;
    return r;
}

SimbriefPlan plan() {
    std::string error;
    const auto p = parseOfp(fixture("ofp_v2_min.json"), &error);
    EXPECT_TRUE(p.has_value()) << error;
    return p.value_or(SimbriefPlan{});
}

PlanKey key(const std::string& plannedAt = "2026-11-23T14:00:00Z") {
    return {"KBFI", "KSAN", "730097_1790536204614", "N814SX", plannedAt};
}

}  // namespace

TEST(SimBrief, DispatchUrl) {
    EXPECT_EQ(dispatchUrl(kbfiKsan()),
              "https://dispatch.simbrief.com/options/custom?orig=KBFI&dest=KSAN&airline=XLY&fltnum=101"
              "&type=730097_1790536204614&pax=46&cargo=0&units=LBS&origrwy=14R&destrwy=27");
}

TEST(SimBrief, DispatchUrlWithoutRunways) {
    DispatchRequest r = kbfiKsan();
    r.originRunway.clear();
    r.destinationRunway.clear();
    const std::string url = dispatchUrl(r);
    EXPECT_EQ(url.find("origrwy"), std::string::npos) << url;
    EXPECT_EQ(url.find("destrwy"), std::string::npos) << url;
    EXPECT_TRUE(url.ends_with("&units=LBS")) << url;
}

TEST(SimBrief, CargoInThousands) {  // checked by Dave: cargo=5 is 5,000 lb (docs/notes/simbrief-api.md)
    DispatchRequest r = kbfiKsan();
    r.cargoLb = 5200;
    EXPECT_NE(dispatchUrl(r).find("&cargo=5.2&"), std::string::npos) << dispatchUrl(r);
    r.cargoLb = 5234;
    EXPECT_NE(dispatchUrl(r).find("&cargo=5.234&"), std::string::npos) << dispatchUrl(r);
    r.cargoLb = 913;
    EXPECT_NE(dispatchUrl(r).find("&cargo=0.913&"), std::string::npos) << dispatchUrl(r);
}

TEST(SimBrief, DispatchUrlEncodesValues) {
    DispatchRequest r = kbfiKsan();
    r.airframeId = "a b&c";
    EXPECT_NE(dispatchUrl(r).find("&type=a%20b%26c&"), std::string::npos) << dispatchUrl(r);
}

TEST(SimBrief, OfpFetchUrl) {
    EXPECT_EQ(ofpFetchUrl("730097"),
              "https://www.simbrief.com/api/xml.fetcher.php?userid=730097&json=v2&client=BlockTime");
}

TEST(SimBrief, ParsesOfp) {
    const SimbriefPlan p = plan();
    EXPECT_EQ(p.requestId, "188511307");
    EXPECT_EQ(p.generatedAt, "2026-11-23T14:02:00Z");
    EXPECT_EQ(p.originIcao, "KBFI");
    EXPECT_EQ(p.destinationIcao, "KSAN");
    EXPECT_EQ(p.airframeId, "730097_1790536204614");
    EXPECT_EQ(p.units, "lbs");
    EXPECT_EQ(p.route, "BANGR9 BANGR DCT LKV DCT OAL J65 BLH DCT JLI SEAVU2");
    EXPECT_EQ(p.originRunway, "14R");
    EXPECT_EQ(p.destinationRunway, "27");
    EXPECT_EQ(p.cruiseAltitudeFt, 37000);
    EXPECT_EQ(p.estBlockS, 9060);
    EXPECT_EQ(p.rampFuelLb, 14200);
    EXPECT_EQ(p.enrouteBurnLb, 10900);
    EXPECT_EQ(p.estLandingWeightLb, 125400);
    EXPECT_EQ(p.originTransitionAltitudeFt, 18000);
    EXPECT_EQ(p.destinationTransitionLevelFt, 18000);
}

TEST(SimBrief, OfpErrorStatus) {
    std::string error;
    EXPECT_FALSE(parseOfp(R"({"fetch":{"userid":"","static_id":"","status":"Error: Unknown UserID","time":"0"}})",
                          &error)
                     .has_value());
    EXPECT_EQ(error, "Error: Unknown UserID");
}

TEST(SimBrief, OfpMissingFieldOrNotJson) {
    std::string error;
    std::string text = fixture("ofp_v2_min.json");
    text.replace(text.find("\"enroute_burn\""), 14, "\"other_burn\"");
    EXPECT_FALSE(parseOfp(text, &error).has_value());
    EXPECT_EQ(error, "SimBrief plan has no fuel.enroute_burn");
    EXPECT_FALSE(parseOfp("<html>Service unavailable</html>", &error).has_value());
    EXPECT_EQ(error, "SimBrief's answer wasn't a plan");
}

TEST(SimBrief, PlanMismatchUnits) {
    SimbriefPlan p = plan();
    p.units = "kgs";
    EXPECT_EQ(planMismatch(p, key()), "The SimBrief plan is in kilograms; BlockTime needs pounds.");
}

TEST(SimBrief, PlanMismatchAirports) {
    SimbriefPlan p = plan();
    p.destinationIcao = "KSEA";
    EXPECT_EQ(planMismatch(p, key()), "The latest SimBrief plan is KBFI–KSEA; this flight is KBFI–KSAN.");
}

TEST(SimBrief, PlanMismatchAirframe) {
    SimbriefPlan p = plan();
    p.airframeId = "80_1722530034323";
    EXPECT_EQ(planMismatch(p, key()), "The latest SimBrief plan isn't for N814SX's SimBrief airframe.");
}

TEST(SimBrief, PlanMismatchStale) {  // generated 14:02Z; stale when before plannedAt minus 5 minutes
    const SimbriefPlan p = plan();
    EXPECT_FALSE(planMismatch(p, key("2026-11-23T14:00:00Z")).has_value());
    EXPECT_FALSE(planMismatch(p, key("2026-11-23T14:07:00Z")).has_value());
    EXPECT_EQ(planMismatch(p, key("2026-11-23T14:08:00Z")),
              "The latest SimBrief plan was generated at 14:02Z, before this flight was planned. Press Generate on "
              "SimBrief, then import again.");
}

TEST(SimBrief, PlanFits) { EXPECT_FALSE(planMismatch(plan(), key()).has_value()); }
