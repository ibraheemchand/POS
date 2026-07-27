#pragma once

#include "core/types.h"
#include <QByteArray>
#include <QList>
#include <QString>

namespace pos {

struct ThermalReceiptItem {
    QString name;
    Quantity quantity{};
    Money lineTotal{};
};

class ThermalPrintService {
public:
    static QByteArray receiptBytes(const QString& storeName, const QString& invoiceNo,
                                   const QList<ThermalReceiptItem>& items, Money total);
    static QByteArray barcodeLabelBytes(const QString& label, const QString& barcode);
    static void writeRaw(const QString& devicePath, const QByteArray& bytes);
};

} // namespace pos
