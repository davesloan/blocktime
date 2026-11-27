// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The aircraft screen (D-241, docs/fleet.md): one airplane's values in four cards (layout, weights, SimBrief, record
// and lease), its lease month, and its two actions: New screenshot (re-read its SimBrief airframe, showing only what
// changes before saving) and Return airplane (with the refund for the unused days). QML only displays.

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include "catalog.h"
#include "dates.h"
#include "fleet_rules.h"
#include "review_client.h"
#include "sqlite_db.h"

class AircraftModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the app")
    Q_PROPERTY(QString registration READ registration NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)            // "737-800 BBJ2 VIP"
    Q_PROPERTY(QString subtitle READ subtitle NOTIFY changed)      // "tail 1001 · flight number 8"
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)  // "Ready at KSAN"
    Q_PROPERTY(QString glyph READ glyph NOTIFY changed)            // as the Fleet card's
    Q_PROPERTY(QString leaseMonthText READ leaseMonthText NOTIFY changed)  // "6 of 30 days"
    Q_PROPERTY(double leaseMonthProgress READ leaseMonthProgress NOTIFY changed)
    // Each card: { source, rows: [{label, value}] }.
    Q_PROPERTY(QVariantMap layoutCard READ layoutCard NOTIFY changed)
    Q_PROPERTY(QVariantMap weightsCard READ weightsCard NOTIFY changed)
    Q_PROPERTY(QVariantMap simbriefCard READ simbriefCard NOTIFY changed)
    Q_PROPERTY(QVariantMap recordCard READ recordCard NOTIFY changed)
    Q_PROPERTY(QVariantList holds READ holds NOTIFY changed)  // {name, lb, share}
    Q_PROPERTY(bool canReturn READ canReturn NOTIFY changed)
    Q_PROPERTY(bool canReread READ canReread NOTIFY changed)
    Q_PROPERTY(QString blockedText READ blockedText NOTIFY changed)  // why both are greyed, or ""
    Q_PROPERTY(QString returnText READ returnText NOTIFY changed)    // the confirmation's sentence
    Q_PROPERTY(QString rereadState READ rereadState NOTIFY changed)  // "idle" | "reading" | "read" | "failed"
    Q_PROPERTY(QString rereadError READ rereadError NOTIFY changed)
    Q_PROPERTY(QVariantList rereadChanges READ rereadChanges NOTIFY changed)  // {label, before, after}
    Q_PROPERTY(QString rereadRefusal READ rereadRefusal NOTIFY changed)
    Q_PROPERTY(QVariantList rereadLayouts READ rereadLayouts NOTIFY changed)  // {name, detail, enabled}
    Q_PROPERTY(int rereadLayoutIndex READ rereadLayoutIndex WRITE setRereadLayoutIndex NOTIFY changed)

public:
    AircraftModel(blocktime::db::Database& db, const blocktime::Catalog& catalog, ReviewClient& claude,
                  std::function<blocktime::Date()> today = blocktime::utcToday, QObject* parent = nullptr);

    // Shows the airplane in the fleet with this registration; false if there's none.
    Q_INVOKABLE bool open(const QString& registration);
    Q_INVOKABLE void reload();

    Q_INVOKABLE void readImage(const QByteArray& bytes, const QString& mediaType);
    Q_INVOKABLE void readImageFile(const QUrl& file);
    Q_INVOKABLE void readClipboard();
    Q_INVOKABLE void cancelReread();
    Q_INVOKABLE bool saveReread();
    Q_INVOKABLE bool returnPlane();

    QString registration() const;
    QString title() const;
    QString subtitle() const;
    QString statusText() const;
    QString glyph() const;
    QString leaseMonthText() const;
    double leaseMonthProgress() const;
    QVariantMap layoutCard() const;
    QVariantMap weightsCard() const;
    QVariantMap simbriefCard() const;
    QVariantMap recordCard() const;
    QVariantList holds() const;
    bool canReturn() const;
    bool canReread() const;
    QString blockedText() const;
    QString returnText() const;
    QString rereadState() const { return rereadState_; }
    QString rereadError() const { return rereadError_; }
    QVariantList rereadChanges() const;
    QString rereadRefusal() const;
    QVariantList rereadLayouts() const;
    int rereadLayoutIndex() const { return rereadLayoutIndex_; }
    void setRereadLayoutIndex(int index);

signals:
    void changed();
    void returned();
    void saved();  // a new screenshot's values were saved

private:
    struct Detail {
        std::int64_t id = 0;
        std::string registration, tail, flightNumber, typeCode, layoutName, location, simbriefId;
        int fuelLb = 0, flights = 0;
        double hours = 0;
        bool awaitingDelivery = false, planned = false;
        blocktime::Date leasedOn;
        blocktime::Cents monthlyCents = 0;
        blocktime::AirplaneLimits limits;
        std::optional<blocktime::SimbriefReading> simbrief;
        std::string readAt;
        std::vector<blocktime::Cabin> cabins;
        std::vector<blocktime::Hold> holds;
    };

    std::optional<Detail> load(const std::string& registration) const;
    const blocktime::CatalogAirframe* rereadLayout() const;
    std::pair<blocktime::Date, blocktime::Date> leaseMonth() const;

    blocktime::db::Database& db_;
    const blocktime::Catalog& catalog_;
    ReviewClient& claude_;
    std::function<blocktime::Date()> today_;
    std::optional<Detail> detail_;

    QString rereadState_ = QStringLiteral("idle");
    QString rereadError_;
    int attempt_ = 0;
    blocktime::SimbriefReading reading_;
    std::vector<blocktime::LayoutChoice> rereadChoices_;
    int rereadLayoutIndex_ = -1;
};
