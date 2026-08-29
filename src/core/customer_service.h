#pragma once
#include "core/types.h"
#include <memory>
namespace pos {
class Database;
struct Customer { QString id; QString name; QString phone; Money creditLimit{}; Money balance{}; int paymentTermsDays{}; bool archived{}; };
struct InvoiceSummary { QString id; QString invoiceNo; Money due{}; };

class CustomerService {
public:
    explicit CustomerService(std::shared_ptr<Database> database);
    QString create(const Customer& customer);
    void archive(const QString& customerId);
    void update(const Customer& customer);
    QList<Customer> listActive() const;
    QList<InvoiceSummary> unpaidInvoices(const QString& customerId) const;
    Money totalReceivables() const;
private: std::shared_ptr<Database> db_;
};
} // namespace pos
