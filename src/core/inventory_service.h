#pragma once
#include "core/types.h"
#include <memory>

namespace pos {
class Database;
struct StockReceipt { QString productId; Quantity quantity{}; Money purchasePrice{}; QString enteredUnit{"base"}; QString batchNo; QDate expiryDate; QString performedBy; };
struct ProductDefinition { QString name; QString sku; QString barcode; QString categoryId; QString brandId; QString description; QString baseUnit; Money purchasePrice{}; Money retailPrice{}; Money wholesalePrice{}; Money dealerPrice{}; Quantity minimumStock{}; bool trackBatches{}; bool trackExpiry{}; QString imagePath; };
struct UnitConversion { QString name; Quantity factor{}; };
struct StockAdjustment { QString productId; QString batchId; Quantity quantityDelta{}; QString type; QString reason; QString performedBy; };
struct InventoryAlert { QString productId; QString productName; Quantity quantity{}; QString batchId; QDate expiryDate; };

class InventoryService {
public:
    explicit InventoryService(std::shared_ptr<Database> database);
    QString createProduct(const QString& name, const QString& baseUnit, Money purchasePrice, Money retailPrice, bool trackBatches);
    QString createProduct(const ProductDefinition& product);
    void updateProduct(const QString& productId, const ProductDefinition& product);
    void archiveProduct(const QString& productId);
    QString cloneProduct(const QString& productId, const QString& newName, const QString& newSku, const QString& newBarcode);
    QString createCategory(const QString& name);
    QString createBrand(const QString& name);
    QString createUnit(const QString& name, const QString& symbol);
    void archiveCategory(const QString& categoryId);
    void archiveBrand(const QString& brandId);
    void setUnitConversions(const QString& productId, const QList<UnitConversion>& conversions);
    Quantity toBaseQuantity(const QString& productId, const QString& unitName, Quantity enteredQuantity) const;
    void adjustStock(const StockAdjustment& adjustment);
    QList<InventoryAlert> lowStock() const;
    QList<InventoryAlert> nearExpiry(const QDate& until) const;
    void receiveStock(const StockReceipt& receipt);
private:
    std::shared_ptr<Database> db_;
};
} // namespace pos
