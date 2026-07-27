#pragma once
#include "core/types.h"
#include <memory>
namespace pos { class Database; struct SuspendedLine { QString productId; Quantity quantity{}; Money unitPrice{}; QString unit; }; struct SuspendedSale { QString id; QString createdAt; QList<SuspendedLine> lines; }; class SuspendedSaleService { public: explicit SuspendedSaleService(std::shared_ptr<Database> database); QString save(const QList<SuspendedLine>& lines); QList<SuspendedSale> list() const; SuspendedSale load(const QString& id) const; void remove(const QString& id); private: std::shared_ptr<Database> db_; }; }
