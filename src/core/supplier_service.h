#pragma once
#include "core/types.h"
#include <memory>
namespace pos { class Database; struct Supplier { QString id; QString name; QString contactPerson; QString phone; QString address; Money openingBalance{}; bool archived{}; }; class SupplierService { public: explicit SupplierService(std::shared_ptr<Database> database); QString create(const Supplier& supplier); void update(const Supplier& supplier); void archive(const QString& supplierId); Supplier find(const QString& supplierId) const; private: std::shared_ptr<Database> db_; }; }
