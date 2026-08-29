#include "core/report_service.h"
#include "core/database.h"
namespace pos { ReportService::ReportService(std::shared_ptr<Database> db):db_(std::move(db)){} BusinessSummary ReportService::summary(const QDate& from,const QDate& to)const{if(!from.isValid()||!to.isValid()||from>to)throw DatabaseError("invalid report dates");BusinessSummary r;const auto a=from.toString(Qt::ISODate),b=to.addDays(1).toString(Qt::ISODate);auto scalar=[this](const char* sql){auto q=db_->prepare(sql);q.stepRow();return q.integer(0);};auto sales=db_->prepare("SELECT COALESCE(SUM(total_paisa),0) FROM sales WHERE status!='voided' AND created_at>=? AND created_at<?");sales.bind(1,a);sales.bind(2,b);sales.stepRow();r.sales=sales.integer(0);auto purchases=db_->prepare("SELECT COALESCE(SUM(total_paisa),0) FROM purchases WHERE status='completed' AND purchased_at>=? AND purchased_at<?");purchases.bind(1,a);purchases.bind(2,b);purchases.stepRow();r.purchases=purchases.integer(0);r.receivables=scalar("SELECT COALESCE(SUM(balance_paisa),0) FROM customers WHERE is_deleted=0");r.payables=scalar("SELECT COALESCE(SUM(balance_paisa),0) FROM suppliers WHERE is_archived=0");r.inventoryValue=scalar("SELECT COALESCE(SUM(stock_quantity*purchase_price_paisa),0) FROM products WHERE is_deleted=0");r.lowStock=scalar("SELECT COUNT(*) FROM products WHERE is_deleted=0 AND stock_quantity<=minimum_stock");return r;}

Money ReportService::todayCashSales(const QDate& date) const {
    const auto a = date.toString(Qt::ISODate);
    const auto b = date.addDays(1).toString(Qt::ISODate);
    auto ct = db_->prepare("SELECT COALESCE(SUM(total_paisa), 0) FROM sales WHERE status!='voided' AND payment_method='cash' AND created_at>=? AND created_at<?");
    ct.bind(1, a);
    ct.bind(2, b);
    ct.stepRow();
    return ct.integer(0);
}

qint64 ReportService::salesCount(const QDate& date) const {
    const auto a = date.toString(Qt::ISODate);
    const auto b = date.addDays(1).toString(Qt::ISODate);
    auto sc = db_->prepare("SELECT COUNT(*) FROM sales WHERE status!='voided' AND created_at>=? AND created_at<?");
    sc.bind(1, a);
    sc.bind(2, b);
    sc.stepRow();
    return sc.integer(0);
}

qint64 ReportService::expiringBatchesCount(const QDate& from, const QDate& to) const {
    auto ex = db_->prepare("SELECT COUNT(*) FROM batches WHERE quantity_remaining>0 AND expiry_date IS NOT NULL AND expiry_date>=? AND expiry_date<=?");
    ex.bind(1, from.toString(Qt::ISODate));
    ex.bind(2, to.toString(Qt::ISODate));
    ex.stepRow();
    return ex.integer(0);
}

QList<QPair<QString, Money>> ReportService::salesTrend(const QDate& from, const QDate& to) const {
    QList<QPair<QString, Money>> result;
    auto q = db_->prepare("SELECT substr(created_at,1,10), COALESCE(SUM(total_paisa),0) FROM sales WHERE status!='voided' AND created_at>=? AND created_at<? GROUP BY substr(created_at,1,10) ORDER BY substr(created_at,1,10)");
    q.bind(1, from.toString(Qt::ISODate));
    q.bind(2, to.addDays(1).toString(Qt::ISODate));
    while (q.stepRow()) {
        result.append({q.text(0), q.integer(1)});
    }
    return result;
}

QList<RecentActivityItem> ReportService::recentSales(int limit) const {
    QList<RecentActivityItem> result;
    auto s = db_->prepare("SELECT invoice_no, total_paisa, created_at FROM sales WHERE status!='voided' ORDER BY created_at DESC LIMIT ?");
    s.bind(1, static_cast<qint64>(limit));
    while (s.stepRow()) {
        result.append({s.text(0), s.integer(1), s.text(2)});
    }
    return result;
}

QList<RecentActivityItem> ReportService::recentPurchases(int limit) const {
    QList<RecentActivityItem> result;
    auto p = db_->prepare("SELECT invoice_no, total_paisa, purchased_at FROM purchases WHERE status='completed' ORDER BY purchased_at DESC LIMIT ?");
    p.bind(1, static_cast<qint64>(limit));
    while (p.stepRow()) {
        result.append({p.text(0), p.integer(1), p.text(2)});
    }
    return result;
}
}
