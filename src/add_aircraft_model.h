// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Add Aircraft (D-242, D-245, docs/fleet.md): a screenshot of the airplane's SimBrief airframe page is read by Claude,
// the values are shown for checking (and fixing a misread), the layout is picked from the aircraft files by seat count,
// and the airplane is leased with a factory or instant delivery. QML only displays; the checks are leasing.h's.

#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "catalog.h"
#include "dates.h"
#include "fleet_rules.h"
#include "leasing.h"
#include "rates.h"
#include "review_client.h"
#include "sqlite_db.h"

class AddAircraftModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the app")
    Q_PROPERTY(QString state READ state NOTIFY changed)  // "empty" | "reading" | "read" | "failed"
    Q_PROPERTY(QString readError READ readError NOTIFY changed)
    Q_PROPERTY(QVariantList fields READ fields NOTIFY changed)  // {key, label, value, missing}
    Q_PROPERTY(QVariantList checks READ checks NOTIFY changed)  // {ok, text}
    Q_PROPERTY(QVariantList layouts READ layouts NOTIFY changed)  // {name, detail, enabled}
    Q_PROPERTY(int layoutIndex READ layoutIndex WRITE setLayoutIndex NOTIFY changed)
    Q_PROPERTY(QString holdsText READ holdsText NOTIFY changed)
    Q_PROPERTY(QVariantList holds READ holds NOTIFY changed)  // {name, lb, share}
    Q_PROPERTY(QString flightNumber READ flightNumber WRITE setFlightNumber NOTIFY changed)
    Q_PROPERTY(QString delivery READ delivery WRITE setDelivery NOTIFY changed)  // "factory" | "instant"
    Q_PROPERTY(QString deliveryIcao READ deliveryIcao WRITE setDeliveryIcao NOTIFY changed)
    Q_PROPERTY(QString factoryText READ factoryText NOTIFY changed)
    Q_PROPERTY(QString deliveryCheck READ deliveryCheck NOTIFY changed)
    Q_PROPERTY(QVariantList deliveryLines READ deliveryLines NOTIFY changed)
    Q_PROPERTY(QString deliveryTotalText READ deliveryTotalText NOTIFY changed)
    Q_PROPERTY(QVariantList leaseLines READ leaseLines NOTIFY changed)  // {label, amount}
    Q_PROPERTY(QString cashAfterText READ cashAfterText NOTIFY changed)
    Q_PROPERTY(QString refusal READ refusal NOTIFY changed)  // "" when Lease can be pressed
    Q_PROPERTY(QString leaseButtonText READ leaseButtonText NOTIFY changed)
    Q_PROPERTY(bool canRetry READ canRetry NOTIFY changed)  // a failed read that sending again could fix

public:
    AddAircraftModel(blocktime::db::Database& db, const blocktime::Catalog& catalog, std::filesystem::path ratesFile,
                     ReviewClient& claude, std::function<blocktime::Date()> today = blocktime::utcToday,
                     QObject* parent = nullptr);

    // Starts over: no screenshot, nothing typed.
    Q_INVOKABLE void clear();
    // Sends a screenshot (PNG or JPEG bytes) to Claude. Without a key, the fields are left for Dave to type.
    Q_INVOKABLE void readImage(const QByteArray& bytes, const QString& mediaType);
    Q_INVOKABLE void readImageFile(const QUrl& file);
    Q_INVOKABLE void readClipboard();  // a screenshot copied with Win+Shift+S
    Q_INVOKABLE void retry();
    // Fixes one value Claude misread (or types one it couldn't read). Numbers may have thousands separators.
    Q_INVOKABLE void setField(const QString& key, const QString& value);
    Q_INVOKABLE bool lease();

    QString state() const { return state_; }
    QString readError() const { return readError_; }
    QVariantList fields() const;
    QVariantList checks() const;
    QVariantList layouts() const;
    int layoutIndex() const { return layoutIndex_; }
    void setLayoutIndex(int index);
    QString holdsText() const;
    QVariantList holds() const;
    QString flightNumber() const { return flightNumber_; }
    void setFlightNumber(const QString& number);
    QString delivery() const { return delivery_; }
    void setDelivery(const QString& delivery);
    QString deliveryIcao() const { return deliveryIcao_; }
    void setDeliveryIcao(const QString& icao);
    QString factoryText() const;
    QString deliveryCheck() const;
    QVariantList deliveryLines() const;
    QString deliveryTotalText() const;
    QVariantList leaseLines() const;
    QString cashAfterText() const;
    QString refusal() const;
    QString leaseButtonText() const;
    bool canRetry() const;

signals:
    void changed();
    void leased(const QString& registration);

private:
    void send();
    void refreshLayouts();
    const blocktime::CatalogType* type() const;
    const blocktime::CatalogAirframe* layout() const;
    std::optional<blocktime::Preview> deliveryPrice() const;
    blocktime::LeaseRequest request() const;

    blocktime::db::Database& db_;
    const blocktime::Catalog& catalog_;
    blocktime::Rates rates_;
    ReviewClient& claude_;
    std::function<blocktime::Date()> today_;

    QString state_ = QStringLiteral("empty");
    QString readError_;
    QByteArray image_;
    QString mediaType_;
    int attempt_ = 0;  // an answer for an older attempt is dropped
    blocktime::SimbriefReading reading_;
    std::vector<blocktime::LayoutChoice> layouts_;
    int layoutIndex_ = -1;
    QString flightNumber_;
    QString delivery_ = QStringLiteral("factory");
    QString deliveryIcao_;
    QString leaseError_;  // a refusal from the lease itself
};
