// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Pre-Flight panel's departure conditions, read from SayIntentions' ATIS and its METAR (D-153, D-188). The texts
// are the ones logged in the KSAN → KSJC fixture.

#include <gtest/gtest.h>

#include "departure_conditions.h"

using namespace blocktime;

namespace {

constexpr const char* kKsanAtis =
    "SAN DIEGO INTL ARPT, INFO LIMA. 1751Z. ARVG AND DPTG RWY 27. WIND CALM. VIS 10. FEW AT 20000, BKN AT 25000. "
    "TEMP 19, DP 5. ALTIM 30.09. DENSITY ALT 300 FT.";
constexpr const char* kKsanMetar =
    "KSAN 251751Z 00000KT 10SM FEW200 BKN250 19/05 A3009 RMK AO2 SLP190 T01890050 10189 20111 51006 $";
constexpr const char* kKsjcAtis =
    "NORMAN Y. MINETA SAN JOSE INTL ARPT, INFO JULIET. 1853Z. ARVG AND DPTG RWYS 30L, 30R. WIND VRB AT 4. VIS 10.";

}  // namespace

TEST(Conditions, KsanLima) {
    const auto w = atisWind(kKsanAtis);
    ASSERT_TRUE(w.has_value());
    EXPECT_TRUE(w->calm);
    EXPECT_EQ(windText(*w), "Calm");
    EXPECT_EQ(metarTemperatureC(kKsanMetar), 19);
    EXPECT_FALSE(metarPrecipitation(kKsanMetar));
    EXPECT_EQ(altimeterText("30.09"), "30.09 inHg");
}

TEST(Conditions, KsjcJuliet) {
    const auto w = atisWind(kKsjcAtis);
    ASSERT_TRUE(w.has_value());
    EXPECT_TRUE(w->variable);
    EXPECT_EQ(w->speedKt, 4);
    EXPECT_EQ(windText(*w), "Variable / 4 kt");
}

TEST(Conditions, SpokenWindWithGust) {
    const auto w = atisWind("INFO KILO. WIND 330 AT 9 GUST 15. VIS 10.");
    ASSERT_TRUE(w.has_value());
    EXPECT_EQ(w->directionDeg, 330);
    EXPECT_EQ(w->speedKt, 9);
    EXPECT_EQ(w->gustKt, 15);
    EXPECT_EQ(windText(*w), "330° / 9G15 kt");
    EXPECT_EQ(windText(*atisWind("WIND 050 AT 12.")), "050° / 12 kt");
    EXPECT_FALSE(atisWind("INFO KILO. VIS 10.").has_value());
}

TEST(Conditions, MetarWindFallback) {
    const auto gust = metarWind("KSEA 251753Z 33009G15KT 10SM");
    ASSERT_TRUE(gust.has_value());
    EXPECT_EQ(gust->directionDeg, 330);
    EXPECT_EQ(gust->speedKt, 9);
    EXPECT_EQ(gust->gustKt, 15);
    const auto vrb = metarWind("KSJC 251853Z VRB04KT 10SM");
    ASSERT_TRUE(vrb.has_value());
    EXPECT_TRUE(vrb->variable);
    EXPECT_EQ(vrb->speedKt, 4);
    EXPECT_TRUE(metarWind(kKsanMetar)->calm);
    EXPECT_FALSE(metarWind("KSAN 251751Z 10SM").has_value());
}

TEST(Conditions, Temperatures) {
    EXPECT_EQ(metarTemperatureC("EGLL 251750Z 24008KT 9999 M02/M05 Q1013"), -2);
    EXPECT_FALSE(metarTemperatureC("KSAN 251751Z 00000KT").has_value());
}

TEST(Conditions, Precipitation) {
    EXPECT_TRUE(metarPrecipitation("KSEA 251753Z 18010KT 5SM -RA BR OVC012 09/08 A2992"));
    EXPECT_TRUE(metarPrecipitation("KMCO 251753Z 27015G25KT 2SM +TSRA BKN020CB 24/22 A2990"));
    EXPECT_TRUE(metarPrecipitation("KDEN 251753Z 36012KT 1SM SHSN OVC008 M03/M05 A3001"));
    EXPECT_TRUE(metarPrecipitation("KORD 251753Z 05008KT 3SM FZDZ OVC010 M01/M02 A3010"));
    EXPECT_FALSE(metarPrecipitation("KSFO 251753Z 28012KT 10SM VCSH FEW020 15/10 A3001"));
    EXPECT_FALSE(metarPrecipitation("KSFO 251753Z 28012KT 4SM BR FEW200 15/10 A3001"));
    EXPECT_FALSE(metarPrecipitation(""));
}

TEST(Conditions, HpaAltimeter) {
    EXPECT_EQ(altimeterText("1013"), "1013 hPa");
    EXPECT_EQ(altimeterText(""), "");
}
