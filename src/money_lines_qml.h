// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The one way money lines reach QML (review 4.4): New Flight's preview, the planned flight's estimate and a settled
// flight's summary.

#pragma once

#include <vector>

#include <QVariantList>

#include "pricing.h"

// Each line as { category, label, amount, working }: amount as formatMoneyFull gives it ("$28,150", "-$660"), working
// as workingText does ("" for lines read back from the ledger).
QVariantList moneyLinesToQml(const std::vector<blocktime::MoneyLine>& lines);
