// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fleet_model.h"

#include <algorithm>
#include <format>

#include "leasing.h"

using blocktime::AirplaneView;

namespace {

std::vector<AirplaneView> inLeaseOrder(const std::vector<AirplaneView>& airplanes) {
    std::vector<AirplaneView> out;
    std::copy_if(airplanes.begin(), airplanes.end(), std::back_inserter(out),
                 [](const AirplaneView& a) { return !a.returnedOn; });
    std::sort(out.begin(), out.end(), [](const AirplaneView& a, const AirplaneView& b) { return a.airplaneId < b.airplaneId; });
    return out;
}

}  // namespace

FleetModel::FleetModel(FleetState state, QObject* parent)
    : QAbstractListModel(parent), state_(std::move(state)), cards_(inLeaseOrder(state_.airplanes)) {}

void FleetModel::reset(FleetState state) {
    beginResetModel();
    state_ = std::move(state);
    cards_ = inLeaseOrder(state_.airplanes);
    endResetModel();
    emit changed();
}

int FleetModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(cards_.size());
}

QVariant FleetModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) return {};
    const AirplaneView& a = cards_[static_cast<std::size_t>(index.row())];
    const QString registration = QString::fromStdString(a.registration);
    const bool mine = registration == state_.plannedRegistration;
    const bool anyPlanned = !state_.plannedRegistration.isEmpty();
    switch (role) {
    case Registration: return registration;
    case Title: return QString::fromStdString(a.airframeName);
    case Place:
        return QString::fromStdString(std::format("At {} · {} lb", a.locationIcao,
                                                  blocktime::formatNumber(a.fuelOnBoardLb)));
    case Leg: return mine ? state_.plannedLeg : QString();
    case Glyph: return mine ? QStringLiteral("current") : a.awaitingDelivery ? QStringLiteral("delivery") : QStringLiteral("ready");
    case Button:
        return mine ? QStringLiteral("Continue Flight") : a.awaitingDelivery ? QStringLiteral("Fly Delivery") : QStringLiteral("Fly");
    case ButtonEnabled: return mine || !anyPlanned;
    case Group: return QString();
    default: return {};
    }
}

QHash<int, QByteArray> FleetModel::roleNames() const {
    return {{Registration, "registration"}, {Title, "title"},   {Place, "place"},   {Leg, "leg"},
            {Glyph, "glyph"},               {Button, "button"}, {ButtonEnabled, "buttonEnabled"}, {Group, "group"}};
}

QString FleetModel::registrationAt(int row) const {
    if (row < 0 || row >= rowCount()) return {};
    return QString::fromStdString(cards_[static_cast<std::size_t>(row)].registration);
}

int FleetModel::slotsOpen() const { return blocktime::slotsEarned(state_.hoursFlown); }

int FleetModel::slotsMax() const { return blocktime::kMaxSlots; }

int FleetModel::emptySlots() const { return std::max(0, slotsOpen() - static_cast<int>(cards_.size())); }

double FleetModel::nextSlotHours() const {
    return slotsOpen() >= blocktime::kMaxSlots ? 0.0 : blocktime::hoursForSlot(slotsOpen() + 1);
}

double FleetModel::nextSlotProgress() const {
    if (slotsOpen() >= blocktime::kMaxSlots) return 1.0;
    const double from = blocktime::hoursForSlot(slotsOpen());
    const double to = nextSlotHours();
    return std::clamp((state_.hoursFlown - from) / (to - from), 0.0, 1.0);
}

QString FleetModel::nextSlotText() const {
    if (slotsOpen() >= blocktime::kMaxSlots) return QStringLiteral("All slots open");
    return QString::fromStdString(std::format("{:.1f} / {:.0f} h", state_.hoursFlown, nextSlotHours()));
}

QVariantList FleetModel::lockedSlotHours() const {
    QVariantList out;
    for (int n = slotsOpen() + 1; n <= blocktime::kMaxSlots; ++n) out.push_back(static_cast<int>(blocktime::hoursForSlot(n)));
    return out;
}

QString FleetModel::hoursText() const { return QString::fromStdString(std::format("{:.1f} h", state_.hoursFlown)); }

QString FleetModel::cashText() const { return QString::fromStdString(blocktime::formatMoneyFull(state_.cashCents)); }

QString FleetModel::leasesText() const {
    blocktime::Cents total = 0;
    for (const AirplaneView& a : cards_) total += a.monthlyLeaseCents;
    return QString::fromStdString(blocktime::formatMoneyFull(total));
}
