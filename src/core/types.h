#pragma once

#include <QString>
#include <QDate>
#include <QList>
#include <QChar>
#include <QRegularExpression>
#include <cmath>

namespace pos {

// Money is handled and displayed in RUPEES, but stored as an exact integer number
// of hundredths of a rupee (0.01) so arithmetic is never subject to floating-point
// error. 100000 == PKR 1,000.00. Convert at the edges: parseMoney() on input,
// formatMoney() on output, roundMoney() after any %/division calculation.
using Money = qint64;
using Quantity = qint64; // base-unit quantity in thousandths, for fractional units.

// Round a rupee value expressed in (possibly fractional) hundredths to the nearest
// whole hundredth, half up. Apply after EVERY money calculation (line totals,
// discounts, commission, partner share, profit, tax, balances).
inline Money roundMoney(double hundredths) { return static_cast<Money>(std::floor(hundredths + 0.5)); }

// Formats a money value (hundredths) as a grouped rupee string WITHOUT the currency
// symbol: "1,000" for whole amounts, "1,000.50" when there are paise. Callers add
// the "PKR " prefix (labels) or leave it bare (price columns), so the displayed
// form is uniformly "PKR 1,000" / "PKR 1,000.50" across screens, receipts and CSV.
inline QString formatMoney(Money hundredths) {
    const bool negative = hundredths < 0;
    const qint64 absolute = negative ? -hundredths : hundredths;
    const qint64 rupees = absolute / 100;
    const int paise = static_cast<int>(absolute % 100);
    QString digits = QString::number(rupees);
    QString grouped;
    int count = 0;
    for (int i = digits.size() - 1; i >= 0; --i) {
        grouped.prepend(digits.at(i));
        if (++count % 3 == 0 && i > 0) grouped.prepend(',');
    }
    QString out = (negative ? "-" : "") + grouped;
    if (paise != 0) out += "." + QString("%1").arg(paise, 2, 10, QChar('0'));
    return out;
}

// Parse rupee text ("1000", "1,000.50", "PKR 25.5") into exact hundredths. Rejects
// more than 2 decimal places. Sets ok=false on any invalid input.
inline Money parseMoney(const QString& text, bool* ok = nullptr) {
    if (ok) *ok = false;
    QString s = text.trimmed();
    s.remove(',');
    s.remove("PKR", Qt::CaseInsensitive);
    s = s.trimmed();
    if (s.isEmpty()) return 0;
    static const QRegularExpression pattern("^-?\\d+(\\.\\d{1,2})?$");
    if (!pattern.match(s).hasMatch()) return 0; // more than 2 decimals, letters, etc.
    const bool negative = s.startsWith('-');
    if (negative) s = s.mid(1);
    const int dot = s.indexOf('.');
    qint64 rupees = 0, paise = 0;
    if (dot < 0) {
        rupees = s.toLongLong();
    } else {
        rupees = s.left(dot).toLongLong();
        QString frac = s.mid(dot + 1);
        if (frac.size() == 1) frac += '0';
        paise = frac.toLongLong();
    }
    Money value = rupees * 100 + paise;
    if (negative) value = -value;
    if (ok) *ok = true;
    return value;
}

struct SaleLine {
    QString productId;
    QString batchId;       // empty selects FEFO automatically for tracked products
    Quantity quantity{};
    Money unitPrice{};
    Money discount{};
    QString unitName;
    // Set only after a manager/owner PIN confirms a discount beyond the commission
    // flexible-margin cap; PosService still re-derives and enforces the cap itself.
    bool discountOverrideApproved{false};
    // Non-empty when this line was loaded as part of a course (bundle). The sale
    // then applies the COURSE's commission settings, grouped once per course,
    // instead of the book's own settings. Kept last so existing aggregate
    // initialisers (which omit it) still compile.
    QString courseId;
};

struct SaleRequest {
    struct Tender { QString method; Money amount{}; };
    QString customerId;
    QString shiftId;
    QString paymentMethod; // cash, credit, cheque, mobile_wallet, mixed
    Money paidAmount{};
    Money invoiceDiscount{};
    QString note;
    QList<SaleLine> lines;
    QList<Tender> tenders;
    // Set only after a manager PIN confirms an invoice discount beyond the cart's
    // allowed flexible margin. PosService re-derives and enforces the limits itself.
    // Kept last so existing aggregate initialisers still compile.
    bool invoiceDiscountOverrideApproved{false};
};

struct SaleResult { QString saleId; QString invoiceNo; Money total{}; };

} // namespace pos
