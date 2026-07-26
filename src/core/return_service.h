#pragma once
#include "core/types.h"
#include <memory>
namespace pos {
class Database;
struct ReturnLine { QString itemId; Quantity quantity{}; };
struct ReturnResult { QString returnId; Money total{}; };
class ReturnService {
public:
    explicit ReturnService(std::shared_ptr<Database> database);
    ReturnResult returnSale(const QString& saleId,const QList<ReturnLine>& lines,const QString& reason,bool refundCash=false);
    ReturnResult returnPurchase(const QString& purchaseId,const QList<ReturnLine>& lines,const QString& reason);
private: std::shared_ptr<Database> db_;
};
}
