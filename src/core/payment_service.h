#pragma once

#include "core/types.h"
#include <memory>

namespace pos {
class Database;
struct InvoicePayment { QString saleId; Money amount{}; };
struct PaymentResult { QString paymentId; Money total{}; };

class PaymentService {
public:
    explicit PaymentService(std::shared_ptr<Database> database);
    PaymentResult recordCustomerPayment(const QString& customerId, const QList<InvoicePayment>& allocations, const QString& method, const QString& note);
private:
    std::shared_ptr<Database> db_;
};
} // namespace pos
