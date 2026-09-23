#pragma once

#include <QWidget>
#include <memory>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QDateEdit>
#include <QTabWidget>

namespace pos { class Database; }

// Owner-only. My Profits + the Profit & Commission report, all computed from the
// saved snapshot rows, net of returns. Behind the owner PIN.
class ProfitReportPage : public QWidget {
    Q_OBJECT
public:
    explicit ProfitReportPage(std::shared_ptr<pos::Database> database, QWidget* parent = nullptr);
public slots:
    void load();
private:
    bool ensureUnlocked();
    void exportCurrentTab();
    std::shared_ptr<pos::Database> database_;
    QDateEdit* fromDate_{};
    QDateEdit* toDate_{};
    QLabel* cardSales_{};
    QLabel* cardCommission_{};
    QLabel* cardPartner_{};
    QLabel* cardProfit_{};
    QLabel* cardPaid_{};
    QLabel* cardOwed_{};
    QTabWidget* tabs_{};
    QTableWidget* courseTable_{};
    QTableWidget* partnerTable_{};
    QTableWidget* bookTable_{};
    QTableWidget* profitsTable_{};
    QLabel* lockedLabel_{};
};
