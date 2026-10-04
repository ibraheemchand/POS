#include "core/receipt_service.h"
#include "core/database.h"
#include "core/settings_service.h"
#include <QPdfWriter>
#include <QPainter>
#include <QPageSize>
#include <QPageLayout>
#include <QMarginsF>
#include <QFont>
#include <QFontMetricsF>
#include <QFileInfo>
#include <QDir>
#include <QImage>
#include <QDateTime>
#include <cmath>

namespace pos {
namespace {

// Always-two-decimals money (grouped), e.g. 195000 -> "1,950.00", 2039 -> "20.39".
QString money2(Money v) {
    const bool neg = v < 0; const qint64 a = neg ? -v : v;
    QString base = formatMoney(a);            // "1,950" or "20.39"
    if (!base.contains('.')) base += ".00";   // force the decimals
    else if (base.section('.', 1).size() == 1) base += "0";
    return (neg ? "-" : "") + base;
}

// English words for 0..999.
QString below1000(int n) {
    static const char* ones[] = {"zero","one","two","three","four","five","six","seven","eight","nine",
        "ten","eleven","twelve","thirteen","fourteen","fifteen","sixteen","seventeen","eighteen","nineteen"};
    static const char* tens[] = {"","","twenty","thirty","forty","fifty","sixty","seventy","eighty","ninety"};
    QString r;
    if (n >= 100) { r += QString(ones[n / 100]) + " hundred"; n %= 100; if (n) r += " "; }
    if (n >= 20) { r += tens[n / 10]; if (n % 10) r += "-" + QString(ones[n % 10]); }
    else if (n > 0) r += ones[n];
    return r;
}
QString intToWords(qint64 n) {
    if (n == 0) return "zero";
    static const char* scale[] = {"", " thousand", " million", " billion", " trillion"};
    QStringList parts; int idx = 0;
    while (n > 0 && idx < 5) {
        const int chunk = int(n % 1000);
        if (chunk) parts.prepend(below1000(chunk) + scale[idx]);
        n /= 1000; ++idx;
    }
    return parts.join(" ");
}

} // namespace

QString ReceiptService::amountToWords(Money hundredths) {
    const qint64 rupees = hundredths / 100;
    const int paisa = int(hundredths % 100);
    QString words = intToWords(rupees) + " rupee" + (rupees == 1 ? "" : "s");
    if (paisa > 0) words += " and " + intToWords(paisa) + " paisa";
    words += " only";
    words[0] = words[0].toUpper();
    return words;
}

ReceiptService::ReceiptService(std::shared_ptr<Database> database) : db_(std::move(database)) {}

ReceiptData ReceiptService::buildFromSale(const QString& saleId) const {
    ReceiptData d;
    SettingsService settings(db_);
    d.storeName = settings.value("business.name", "Invento");
    d.address = settings.value("business.address");
    d.phone = settings.value("business.phone");
    d.note = settings.value("receipt.footer");
    d.cashier = settings.value("receipt.cashier", "Owner");

    auto sale = db_->prepare("SELECT invoice_no, created_at, discount, total, paid, due, customer_id, tendered FROM sales WHERE id=?");
    sale.bind(1, saleId);
    if (!sale.stepRow()) throw DatabaseError("sale not found");
    d.invoiceNo = sale.text(0);
    QDateTime dt = QDateTime::fromString(sale.text(1), Qt::ISODate);
    dt.setTimeSpec(Qt::UTC);
    d.dateTime = dt.isValid() ? dt.toLocalTime().toString("d MMM yyyy, h:mm AP") : sale.text(1);
    d.invoiceDiscount = sale.integer(2);
    d.netTotal = sale.integer(3);
    d.cashReceived = sale.integer(4);
    const Money due = sale.integer(5);
    const QString customerId = sale.text(6);
    const Money tendered = sale.integer(7); // raw cash handed over; 0 if unknown (old sales)

    auto items = db_->prepare(
        "SELECT p.name, si.quantity, si.unit_price, si.line_total FROM sale_items si "
        "JOIN products p ON p.id=si.product_id WHERE si.sale_id=? ORDER BY si.rowid");
    items.bind(1, saleId);
    while (items.stepRow()) {
        ReceiptLine ln;
        ln.name = items.text(0);
        ln.qty = items.integer(1);
        ln.rate = items.integer(2);
        ln.net = items.integer(3);
        ln.disc = ln.qty * ln.rate - ln.net; // line + spread invoice share
        d.totalQty += ln.qty;
        d.gross += ln.qty * ln.rate;
        d.lines.append(ln);
    }
    if (d.lines.isEmpty()) throw DatabaseError("this sale has no line items to print");

    d.totalDiscount = d.gross - d.netTotal;
    d.lineDiscount = d.totalDiscount - d.invoiceDiscount;
    if (d.lineDiscount < 0) d.lineDiscount = 0;
    if (tendered > d.cashReceived) d.cashReceived = tendered; // show the real cash handed over
    d.change = d.cashReceived > d.netTotal ? d.cashReceived - d.netTotal : 0;
    d.amountInWords = amountToWords(d.netTotal);

    if (!customerId.isEmpty()) {
        auto c = db_->prepare("SELECT name, phone, balance FROM customers WHERE id=?");
        c.bind(1, customerId);
        if (c.stepRow()) {
            d.hasCustomer = true;
            d.customerName = c.text(0);
            d.customerPhone = c.text(1);
            d.remainingBalance = c.integer(2);
            d.paidNow = d.cashReceived;
            d.prevBalance = d.remainingBalance - due; // balance already includes this sale's due
        }
    }
    return d;
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
qreal ReceiptService::paint(QPainter& p, qreal widthPx, const ReceiptData& d, double printableMm) {
    const double s = widthPx / printableMm; // px per mm
    const double W = widthPx;
    auto MM = [&](double mm) { return mm * s; };
    QString family = p.font().family();
    auto font = [&](double pt, bool bold = false, bool italic = false) {
        QFont f(family);
        f.setPixelSize(std::max(1, int(std::lround(pt / 72.0 * 25.4 * s))));
        f.setBold(bold); f.setItalic(italic);
        return f;
    };
    const QColor black(0, 0, 0), grey(224, 224, 224);
    QPen line(black); line.setWidthF(std::max(1.0, s * 0.13));
    const bool compact = printableMm < 60;

    const QFont fShop = font(13, true), fBody = font(7.5), fBold = font(7.5, true),
                fItalic = font(7.5, false, true), fSection = font(8, true);
    const double pad = MM(1.0);
    double y = 0;

    auto dashed = [&](double yy) {
        QPen dp(black); dp.setWidthF(std::max(1.0, s * 0.12)); dp.setStyle(Qt::DashLine);
        QVector<qreal> dash; dash << 3 << 3; dp.setDashPattern(dash);
        p.setPen(dp); p.drawLine(QPointF(0, yy), QPointF(W, yy)); p.setPen(line);
    };
    auto centered = [&](const QString& text, const QFont& f, double& yy, bool italic = false) {
        p.setFont(f); QFontMetricsF fm(f);
        QRectF r(0, yy, W, 10000);
        QRectF br = p.boundingRect(r, Qt::AlignHCenter | Qt::TextWordWrap, text);
        p.drawText(r, Qt::AlignHCenter | Qt::TextWordWrap, text);
        yy += br.height();
        Q_UNUSED(italic); Q_UNUSED(fm);
    };

    // --- Header ---
    { double yy = y; centered(d.storeName.trimmed(), fShop, yy); y = yy + MM(0.5); }
    {
        QString sub = d.address.trimmed();
        if (!d.phone.trimmed().isEmpty()) sub += (sub.isEmpty() ? "" : "  |  ") + d.phone.trimmed();
        if (!sub.isEmpty()) { double yy = y; centered(sub, fBody, yy); y = yy; }
    }
    y += MM(1.0); dashed(y); y += MM(1.5);

    // --- Info block ---
    p.setFont(fBody);
    const QFontMetricsF bm(fBody);
    const double lh = bm.height() + MM(0.4);
    const double labelW = MM(compact ? 14 : 16);
    auto info = [&](const QString& label, const QString& value) {
        p.setFont(fBody);
        QRectF vr = p.boundingRect(QRectF(labelW, y, W - labelW, 10000), Qt::AlignLeft | Qt::TextWordWrap, value);
        const double h = std::max(lh, vr.height());
        p.setFont(fBold); p.drawText(QRectF(0, y, labelW, h), Qt::AlignLeft | Qt::AlignTop, label);
        p.setFont(fBody); p.drawText(QRectF(labelW, y, W - labelW, h), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, value);
        y += h;
    };
    info("Invoice:", d.invoiceNo);
    info("Date:", d.dateTime);
    if (d.hasCustomer) {
        QString c = d.customerName;
        if (!d.customerPhone.trimmed().isEmpty()) c += " (" + d.customerPhone + ")";
        info("Customer:", c);
    }
    info("Cashier:", d.cashier);
    y += MM(1.0);

    // --- Items ---
    if (!compact) {
        // Columns as fractions of the given 72mm spec, scaled to W. No per-line
        // Disc column: this shop only uses invoice-level discounts (shown in totals).
        const double fw[5] = {4.0, 27.5, 6.0, 11.0, 23.5};
        double cx[6]; cx[0] = 0; for (int i = 0; i < 5; ++i) cx[i + 1] = cx[i] + W * (fw[i] / 72.0);
        const Qt::Alignment al[5] = {Qt::AlignHCenter, Qt::AlignLeft, Qt::AlignHCenter,
                                     Qt::AlignRight, Qt::AlignRight};
        auto rowHeight = [&](const QString& itemText, const QFont& f) {
            p.setFont(f); QFontMetricsF fm(f);
            QRectF br = fm.boundingRect(QRectF(0, 0, (cx[2] - cx[1]) - 2 * pad, 10000),
                                        Qt::AlignLeft | Qt::TextWordWrap, itemText);
            return std::max(fm.height(), br.height()) + 2 * MM(0.7);
        };
        auto drawRow = [&](const QString cells[5], const QFont& f, bool fill) {
            const double h = rowHeight(cells[1], f);
            if (fill) p.fillRect(QRectF(0, y, W, h), grey);
            p.setPen(line);
            for (int i = 0; i < 5; ++i) {
                p.drawRect(QRectF(cx[i], y, cx[i + 1] - cx[i], h));
                p.setFont(f);
                p.drawText(QRectF(cx[i] + pad, y, (cx[i + 1] - cx[i]) - 2 * pad, h),
                           al[i] | Qt::AlignVCenter | Qt::TextWordWrap, cells[i]);
            }
            y += h;
        };
        const QString head[5] = {"#", "Item", "Qty", "Rate", "Net"};
        drawRow(head, fBold, true);
        int n = 1;
        for (const auto& it : d.lines) {
            const QString row[5] = {QString::number(n++), it.name.trimmed(), QString::number(it.qty),
                                    formatMoney(it.rate), money2(it.net)};
            drawRow(row, fBody, false);
        }
    } else {
        // 58mm compact: name on one line, "Qty x Rate = Net" below.
        p.setPen(line); p.drawLine(QPointF(0, y), QPointF(W, y)); y += MM(1.0);
        int n = 1;
        for (const auto& it : d.lines) {
            p.setFont(fBold); p.drawText(QRectF(0, y, W, lh), Qt::AlignLeft | Qt::AlignVCenter,
                                         QString("%1. %2").arg(n++).arg(it.name.trimmed()));
            y += lh;
            p.setFont(fBody);
            p.drawText(QRectF(MM(2), y, W - MM(2), lh), Qt::AlignLeft | Qt::AlignVCenter,
                       QString("%1 x %2 = %3").arg(it.qty).arg(formatMoney(it.rate), money2(it.net)));
            y += lh;
        }
        p.drawLine(QPointF(0, y), QPointF(W, y));
    }
    y += MM(1.5);

    // --- Totals table (2 columns) ---
    {
        const double lw = W * 0.55, rw = W - lw;
        auto trow = [&](const QString& label, const QString& value, bool boldGrey) {
            p.setFont(boldGrey ? fBold : fBody); QFontMetricsF fm(p.font());
            const double h = fm.height() + 2 * MM(0.7);
            if (boldGrey) p.fillRect(QRectF(0, y, W, h), grey);
            p.setPen(line);
            p.drawRect(QRectF(0, y, lw, h)); p.drawRect(QRectF(lw, y, rw, h));
            p.drawText(QRectF(pad, y, lw - 2 * pad, h), Qt::AlignLeft | Qt::AlignVCenter, label);
            p.drawText(QRectF(lw + pad, y, rw - 2 * pad, h), Qt::AlignRight | Qt::AlignVCenter, value);
            y += h;
        };
        trow("Total qty", QString::number(d.totalQty), false);
        trow("Gross total", money2(d.gross), false);
        if (d.lineDiscount != 0) trow("Line discount", money2(d.lineDiscount), false);
        trow("Invoice discount", money2(d.invoiceDiscount), false);
        trow("Total discount", money2(d.totalDiscount), false);
        trow("NET TOTAL", "PKR " + money2(d.netTotal), true);
        trow("Cash received", money2(d.cashReceived), false);
        trow("Change", money2(d.change), false);
    }
    y += MM(1.5);

    // --- Amount in words ---
    { double yy = y; centered(d.amountInWords, fItalic, yy); y = yy + MM(1.5); }

    // --- Customer account ---
    if (d.hasCustomer) {
        p.setFont(fSection); p.drawText(QRectF(pad, y, W, bm.height() + MM(1)), Qt::AlignLeft, "Customer account");
        y += bm.height() + MM(1.5);
        const double lw = W * 0.6, rw = W - lw;
        auto crow = [&](const QString& label, const QString& value, bool boldGrey) {
            p.setFont(boldGrey ? fBold : fBody); QFontMetricsF fm(p.font());
            const double h = fm.height() + 2 * MM(0.7);
            if (boldGrey) p.fillRect(QRectF(0, y, W, h), grey);
            p.setPen(line);
            p.drawRect(QRectF(0, y, lw, h)); p.drawRect(QRectF(lw, y, rw, h));
            p.drawText(QRectF(pad, y, lw - 2 * pad, h), Qt::AlignLeft | Qt::AlignVCenter, label);
            p.drawText(QRectF(lw + pad, y, rw - 2 * pad, h), Qt::AlignRight | Qt::AlignVCenter, value);
            y += h;
        };
        crow("Previous balance", money2(d.prevBalance), false);
        crow("+ This invoice", money2(d.netTotal), false);
        crow("- Paid now", money2(d.paidNow), false);
        crow(compact ? "REMAINING BAL." : "REMAINING BALANCE", "PKR " + money2(d.remainingBalance), true);
        y += MM(1.5);
    }

    // --- Footer ---
    dashed(y); y += MM(1.5);
    if (!d.note.trimmed().isEmpty()) { double yy = y; centered(d.note.trimmed(), fBody, yy); y = yy + MM(2.0); }
    if (!compact) {
        p.setFont(fBody);
        const double half = W / 2;
        p.drawText(QRectF(0, y, half, lh), Qt::AlignHCenter, "________________");
        p.drawText(QRectF(half, y, half, lh), Qt::AlignHCenter, "________________");
        y += lh;
        p.drawText(QRectF(0, y, half, lh), Qt::AlignHCenter, "Buyer's signature");
        p.drawText(QRectF(half, y, half, lh), Qt::AlignHCenter, "Seller's signature");
        y += lh + MM(1.5);
    }
    { double yy = y; centered("Thank you for your visit!", fBody, yy); y = yy; }
    { double yy = y; centered("Powered by Invento", fBody, yy); y = yy; }
    { double yy = y; centered("Made By IbraheemChand", fBody, yy); y = yy + MM(2.0); }

    return y;
}

void ReceiptService::renderPdf(const ReceiptData& data, const QString& filePath, double paperMm) {
    if (data.lines.isEmpty()) throw DatabaseError("there is nothing to render");
    QFileInfo info(filePath);
    QDir().mkpath(info.absolutePath());

    const double printableMm = paperMm < 60 ? 48.0 : 72.0;
    const double marginMm = (paperMm - printableMm) / 2.0;
    const int measurePx = int(std::lround(printableMm * 8.0)); // 203 dpi = 8 px/mm

    // Pass 1 — measure the height on a throwaway canvas.
    QImage tmp(measurePx, 8000, QImage::Format_RGB32);
    tmp.fill(Qt::white);
    QPainter mp(&tmp);
    const qreal hPx = paint(mp, measurePx, data, printableMm);
    mp.end();
    const double contentMm = hPx / 8.0;
    const double pageHmm = contentMm + 2 * marginMm + 2.0;

    // Pass 2 — the real PDF page sized to the content.
    QPdfWriter writer(filePath);
    writer.setResolution(203);
    writer.setPageSize(QPageSize(QSizeF(paperMm, pageHmm), QPageSize::Millimeter));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    writer.setTitle("Receipt " + data.invoiceNo);
    QPainter p(&writer);
    if (!p.isActive()) throw DatabaseError(QString("could not create the PDF at %1").arg(filePath));
    const double sPdf = writer.width() / paperMm;
    p.translate(marginMm * sPdf, marginMm * sPdf);
    paint(p, printableMm * sPdf, data, printableMm);
    p.end();
}

QByteArray ReceiptService::escposRasterBytes(const ReceiptData& data, double paperMm) {
    const double printableMm = paperMm < 60 ? 48.0 : 72.0;
    const int dots = int(std::lround(printableMm * 8.0)); // 576 (80mm) or 384 (58mm)

    QImage img(dots, 8000, QImage::Format_RGB32);
    img.fill(Qt::white);
    QPainter p(&img);
    const qreal hPx = paint(p, dots, data, printableMm);
    p.end();
    const int height = std::min(8000, int(std::ceil(hPx)) + 4);
    img = img.copy(0, 0, dots, height).convertToFormat(QImage::Format_Grayscale8);

    const int bytesPerRow = (dots + 7) / 8;
    QByteArray out;
    out.append('\x1b').append('@'); // ESC @ reset

    // GS v 0 in bands so tall receipts stay within the command's limits.
    const int band = 128;
    for (int y0 = 0; y0 < height; y0 += band) {
        const int rows = std::min(band, height - y0);
        out.append('\x1d').append('v').append('0').append('\x00');
        out.append(char(bytesPerRow & 0xff)).append(char((bytesPerRow >> 8) & 0xff));
        out.append(char(rows & 0xff)).append(char((rows >> 8) & 0xff));
        for (int y = 0; y < rows; ++y) {
            const uchar* scan = img.constScanLine(y0 + y);
            for (int bx = 0; bx < bytesPerRow; ++bx) {
                uchar bits = 0;
                for (int bit = 0; bit < 8; ++bit) {
                    const int x = bx * 8 + bit;
                    if (x < dots && scan[x] < 128) bits |= (0x80 >> bit); // dark pixel
                }
                out.append(char(bits));
            }
        }
    }
    out.append('\n').append('\x1b').append('d').append('\x03'); // feed
    out.append('\x1d').append('V').append('\x01');              // partial cut
    return out;
}

} // namespace pos
