#pragma once

#include "core/types.h"
#include <memory>

namespace pos {
class Database;

struct PurchaseLine {
    QString productId;
    Quantity quantity{};
    Money unitPrice{};
    Money discount{};
    Money tax{};
    QString unitName;
    QString batchNo;
    QDate expiryDate;
};

struct PurchaseRequest {
    QString supplierId;
    QString warehouseId;
    QString paymentMethod{"credit"};
    Money paidAmount{};
    Money invoiceDiscount{};
    Money invoiceTax{};
    QString note;
    QList<PurchaseLine> lines;
    QString shiftId;
};

struct PurchaseResult { QString purchaseId; QString invoiceNo; Money total{}; };

class PurchaseService {
public:
    explicit PurchaseService(std::shared_ptr<Database> database);
    PurchaseResult completePurchase(const PurchaseRequest& request);
private:
    std::shared_ptr<Database> db_;
};
} // namespace pos
