#include "core/receipt_service.h"
#include "core/database.h"
#include "core/settings_service.h"
#include <QPdfWriter>
#include <QPainter>
#include <QPageSize>
#include <QFont>
#include <QFileInfo>
#include <QDir>

namespace pos {

ReceiptService::ReceiptService(std::shared_ptr<Database> database) : db_(std::move(database)) {}

ReceiptData ReceiptService::buildFromSale(const QString& saleId) const {
    ReceiptData data;
    SettingsService settings(db_);
    data.storeName = settings.value("business.name", "Invento");
    data.footer = settings.value("receipt.footer");

    auto sale = db_->prepare("SELECT invoice_no, created_at, total FROM sales WHERE id=?");
    sale.bind(1, saleId);
    if (!sale.stepRow()) throw DatabaseError("sale not found");
    data.invoiceNo = sale.text(0);
    data.dateTime = sale.text(1);
    data.total = sale.integer(2);

    auto items = db_->prepare(
        "SELECT p.name, si.quantity, si.line_total FROM sale_items si "
        "JOIN products p ON p.id=si.product_id WHERE si.sale_id=? ORDER BY si.rowid");
    items.bind(1, saleId);
    while (items.stepRow()) {
        data.items.append({items.text(0), items.integer(1), items.integer(2)});
    }
    if (data.items.isEmpty()) throw DatabaseError("this sale has no line items to print");
    return data;
}

void ReceiptService::paint(QPainter& painter, const QRectF& area, const ReceiptData& data) {
    painter.save();
    const int x = static_cast<int>(area.left());
    const int right = static_cast<int>(area.right());
    int y = static_cast<int>(area.top());

    QFont title = painter.font(); title.setPointSize(16); title.setBold(true);
    QFont normal = painter.font(); normal.setPointSize(10); normal.setBold(false);
    QFont bold = normal; bold.setBold(true);
    const QFontMetrics tm(title), nm(normal);

    painter.setFont(title);
    painter.drawText(QRect(x, y, right - x, tm.height() + 6), Qt::AlignHCenter, data.storeName.trimmed());
    y += tm.height() + 10;

    painter.setFont(normal);
    const int lh = nm.height() + 6;
    painter.drawText(x, y + nm.ascent(), QString("Invoice: %1").arg(data.invoiceNo)); y += lh;
    if (!data.dateTime.trimmed().isEmpty()) { painter.drawText(x, y + nm.ascent(), QString("Date: %1").arg(data.dateTime)); y += lh; }
    painter.drawLine(x, y, right, y); y += 8;

    for (const auto& item : data.items) {
        const QString left = QString("%1 x%2").arg(item.name.trimmed()).arg(item.quantity);
        const QString amount = "PKR " + formatMoney(item.lineTotal);
        painter.drawText(x, y + nm.ascent(), left);
        painter.drawText(QRect(x, y, right - x, lh), Qt::AlignRight, amount);
        y += lh;
    }
    painter.drawLine(x, y, right, y); y += 8;

    painter.setFont(bold);
    const QFontMetrics bm(bold);
    painter.drawText(x, y + bm.ascent(), "TOTAL");
    painter.drawText(QRect(x, y, right - x, bm.height() + 6), Qt::AlignRight, "PKR " + formatMoney(data.total));
    y += bm.height() + 12;

    if (!data.footer.trimmed().isEmpty()) {
        painter.setFont(normal);
        painter.drawText(QRect(x, y, right - x, lh * 3), Qt::AlignHCenter | Qt::TextWordWrap, data.footer.trimmed());
    }
    painter.restore();
}

void ReceiptService::renderPdf(const ReceiptData& data, const QString& filePath) {
    if (data.items.isEmpty()) throw DatabaseError("there is nothing to render");
    QFileInfo info(filePath);
    QDir().mkpath(info.absolutePath());

    QPdfWriter writer(filePath);
    writer.setPageSize(QPageSize(QPageSize::A5));
    writer.setResolution(300);
    writer.setTitle("Receipt " + data.invoiceNo);
    QPainter painter(&writer);
    if (!painter.isActive()) throw DatabaseError(QString("could not create the PDF at %1").arg(filePath));
    const QRectF page(0, 0, writer.width(), writer.height());
    const QRectF margins = page.adjusted(page.width() * 0.06, page.height() * 0.05, -page.width() * 0.06, -page.height() * 0.05);
    paint(painter, margins, data);
    painter.end();
}

QByteArray ReceiptService::escposBytes(const ReceiptData& data) {
    return ThermalPrintService::receiptBytes(data.storeName, data.invoiceNo, data.items, data.total);
}

} // namespace pos
