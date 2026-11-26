// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Pre-Flight panel's departure conditions (D-153, D-188): wind, temperature, precipitation and the altimeter, read
// from SayIntentions' ATIS text and its METAR. BlockTime calculates no performance (D-118); these are the EFB's inputs.

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace blocktime {

struct Wind {
    bool calm = false, variable = false;
    int directionDeg = 0, speedKt = 0, gustKt = 0;  // gust 0 = none
};

// The spoken ATIS (magnetic in the US): "WIND CALM", "WIND VRB AT 4", "WIND 330 AT 9 GUST 15". nullopt without one.
std::optional<Wind> atisWind(std::string_view atisText);
// The METAR's wind group (true): "00000KT", "VRB04KT", "33009G15KT". nullopt without one.
std::optional<Wind> metarWind(std::string_view metar);
// "Calm", "Variable / 4 kt", "330° / 9 kt", "330° / 9G15 kt".
std::string windText(const Wind& w);
// The METAR's temperature: "19/05" → 19, "M02/M05" → -2.
std::optional<int> metarTemperatureC(std::string_view metar);
// A weather group with drizzle, rain, snow, snow grains, ice pellets, hail or unknown precipitation, on the airport
// (a VC group is near it, so it doesn't count).
bool metarPrecipitation(std::string_view metar);
// "30.09" → "30.09 inHg", "1013" → "1013 hPa", "" → "".
std::string altimeterText(std::string_view altimeter);
// "J" → "JULIETT", "a" → "ALFA": the ICAO word, in ICAO's spelling (D-193, D-201). "" for anything else.
std::string phoneticWord(std::string_view letter);
// The approaches an ATIS has in use: "ILS RWY 30L AND RNAV-Z RWY 30R APCHS IN USE" → "ILS 30L · RNAV-Z 30R"; ""
// without one. Visual approaches are left out.
std::string atisApproaches(std::string_view atisText);
// The ATIS's runway list: "30L,30R" → "30L 30R".
std::string runwayList(std::string_view atisRunways);

}  // namespace blocktime
