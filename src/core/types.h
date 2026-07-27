#pragma once

#include <QString>
#include <QDate>
#include <QList>

namespace pos {

// All money values are paisa (one hundredth of a PKR). Never use float/double.
using Money = qint64;
using Quantity = qint64; // base-unit quantity in thousandths, for fractional units.

struct SaleLine {
    QString productId;
    QString batchId;       // empty selects FEFO automatically for tracked products
    Quantity quantity{};
    Money unitPrice{};
    Money discount{};
    QString unitName;
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
};

struct SaleResult { QString saleId; QString invoiceNo; Money total{}; };

} // namespace pos
