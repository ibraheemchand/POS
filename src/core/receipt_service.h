#pragma once

#include "core/types.h"
#include "core/thermal_print_service.h"
#include <QList>
#include <QString>
#include <memory>

class QPainter;
class QRectF;

namespace pos {
class Database;

// Everything needed to render a receipt in any output mode.
struct ReceiptData {
    QString storeName;
    QString invoiceNo;
    QString dateTime;
    QString footer;
    QList<ThermalReceiptItem> items;
    Money total{};
};

// Builds receipt data (from a saved sale or ad-hoc) and renders it as either a
// real PDF (QPdfWriter) or an ESC/POS byte stream. The QPrinter/Windows-driver
// path lives in the UI layer (needs Qt PrintSupport) and reuses paint().
class ReceiptService {
public:
    explicit ReceiptService(std::shared_ptr<Database> database);

    // Load a completed sale's receipt (invoice, date, lines, total) plus the store
    // name and footer from settings.
    ReceiptData buildFromSale(const QString& saleId) const;

    // Draw the receipt onto any QPainter within `area` (shared by the PDF writer and
    // the Windows-driver printer so both look identical).
    static void paint(QPainter& painter, const QRectF& area, const ReceiptData& data);

    // Write a valid PDF directly (no printer involved).
    static void renderPdf(const ReceiptData& data, const QString& filePath);

    // ESC/POS bytes for a raw thermal printer.
    static QByteArray escposBytes(const ReceiptData& data);

private:
    std::shared_ptr<Database> db_;
};
} // namespace pos
