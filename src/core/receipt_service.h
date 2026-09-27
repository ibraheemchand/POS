#pragma once

#include "core/types.h"
#include <QList>
#include <QString>
#include <QByteArray>
#include <memory>

class QPainter;

namespace pos {
class Database;

// One line of the items table.
struct ReceiptLine {
    QString name;
    qint64 qty{};
    Money rate{}; // unit price
    Money disc{}; // discount attributed to this line (line + spread invoice share)
    Money net{};  // line total after discount
};

// Everything the 80mm/58mm receipt table needs. Money values are hundredths.
struct ReceiptData {
    QString storeName, address, phone;
    QString invoiceNo, dateTime; // dateTime already localised, e.g. "28 Sep 2026, 12:51 AM"
    QString cashier;
    bool hasCustomer{};
    QString customerName, customerPhone;

    QList<ReceiptLine> lines;
    qint64 totalQty{};
    Money gross{};           // sum(qty*rate)
    Money lineDiscount{};    // per-line discounts total
    Money invoiceDiscount{}; // whole-invoice discount
    Money totalDiscount{};   // line + invoice
    Money netTotal{};
    Money cashReceived{};
    Money change{};
    QString amountInWords;

    // Customer account block (only when hasCustomer).
    Money prevBalance{};
    Money paidNow{};
    Money remainingBalance{};

    QString note; // footer note from Settings
};

// Builds receipt data from a sale and renders it three ways from ONE layout:
//  - a real PDF (auto-height single page),
//  - a raster bitmap for raw ESC/POS thermal printers (so the grid prints identically),
//  - (the QPrinter/Windows-driver path reuses paint() from the UI layer).
class ReceiptService {
public:
    explicit ReceiptService(std::shared_ptr<Database> database);

    ReceiptData buildFromSale(const QString& saleId) const;

    // Draw the whole receipt at pixel origin (0,0) across `widthPx`, using
    // `printableMm` to pick the 80mm table layout or the 58mm compact layout.
    // Returns the total pixel height used. Fonts/spacing derive from widthPx/printableMm
    // so PDF and raster come out pixel-identical.
    static qreal paint(QPainter& painter, qreal widthPx, const ReceiptData& data, double printableMm);

    // Valid PDF sized to a single long page for the given paper width (mm).
    static void renderPdf(const ReceiptData& data, const QString& filePath, double paperMm = 80.0);

    // ESC/POS bytes that print the receipt as a bitmap (GS v 0 raster), so grid lines
    // and the exact table layout appear on a raw thermal printer.
    static QByteArray escposRasterBytes(const ReceiptData& data, double paperMm = 80.0);

    static QString amountToWords(Money hundredths); // "One thousand ... rupees and NN paisa only"

private:
    std::shared_ptr<Database> db_;
};
} // namespace pos
