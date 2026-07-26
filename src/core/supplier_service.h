#pragma once
#include "core/types.h"
#include <memory>
namespace pos {
class Database;
struct Supplier { QString id; QString name; QString contactPerson; QString phone; QString address; Money openingBalance{}; bool archived{}; };
struct SupplierLedgerEntry { QString entryType; QString referenceId; Money debit{}; Money credit{}; Money balance{}; QString createdAt; };
class SupplierService {
public:
    explicit SupplierService(std::shared_ptr<Database> database);
    QString create(const Supplier& supplier);
    void update(const Supplier& supplier);
    void archive(const QString& supplierId);
    Supplier find(const QString& supplierId) const;
    QList<SupplierLedgerEntry> ledger(const QString& supplierId) const;
private:
    std::shared_ptr<Database> db_;
};
}
