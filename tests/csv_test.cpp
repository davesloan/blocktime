// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "csv.h"

using namespace blocktime;

TEST(Csv, QuotedCommasNewlinesAndDoubledQuotes) {
    std::istringstream in("a,\"b,c\",\"d\"\"e\",\"f\ng\"\r\nh\n");
    CsvReader csv(in);
    std::vector<std::string> row;
    ASSERT_TRUE(csv.next(row));
    EXPECT_EQ(row, (std::vector<std::string>{"a", "b,c", "d\"e", "f\ng"}));
    ASSERT_TRUE(csv.next(row));
    EXPECT_EQ(row, (std::vector<std::string>{"h"}));
    EXPECT_FALSE(csv.next(row));
}

TEST(Csv, EmptyFieldsAndNoTrailingNewline) {
    std::istringstream in("1,,\"\",x");
    CsvReader csv(in);
    std::vector<std::string> row;
    ASSERT_TRUE(csv.next(row));
    EXPECT_EQ(row, (std::vector<std::string>{"1", "", "", "x"}));
    EXPECT_FALSE(csv.next(row));
}
