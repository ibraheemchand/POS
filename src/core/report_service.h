#pragma once
#include "core/types.h"
#include <memory>
namespace pos { class Database; struct BusinessSummary { Money sales{},purchases{},receivables{},payables{},inventoryValue{}; qint64 lowStock{}; };
struct RecentActivityItem { QString invoiceNo; Money total{}; QString date; };
struct SaleHistoryRow { QString id; QString invoiceNo; QString date; Money total{}; };

class ReportService {
public:
    explicit ReportService(std::shared_ptr<Database> db);
    BusinessSummary summary(const QDate& from,const QDate& to) const;
    Money todayCashSales(const QDate& date) const;
    qint64 salesCount(const QDate& date) const;
    qint64 expiringBatchesCount(const QDate& from, const QDate& to) const;
    QList<QPair<QString, Money>> salesTrend(const QDate& from, const QDate& to) const;
    QList<RecentActivityItem> recentSales(int limit) const;
    QList<RecentActivityItem> recentPurchases(int limit) const;
    QList<SaleHistoryRow> recentSalesDetailed(int limit) const; // with sale id, for reprint
private:
    std::shared_ptr<Database> db_;
};
}
