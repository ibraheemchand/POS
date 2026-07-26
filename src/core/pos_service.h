#pragma once
#include "core/types.h"
#include <memory>

namespace pos {
class Database;

class PosService {
public:
    explicit PosService(std::shared_ptr<Database> database);
    SaleResult completeSale(const SaleRequest& request);
    void voidSale(const QString& saleId, const QString& reason, const QString& performedBy);
private:
    struct BatchAllocation { QString batchId; Quantity quantity{}; };
    std::shared_ptr<Database> db_;
    QList<BatchAllocation> allocateBatches(const SaleLine& line, const QString& saleId, const QString& performedBy);
};
} // namespace pos
