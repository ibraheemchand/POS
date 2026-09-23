#pragma once

#include <QWidget>
#include <memory>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QDateEdit>

namespace pos { class Database; }

// Owner-only. Partner directory with balances, per-partner ledger and payouts.
class PartnersPage : public QWidget {
    Q_OBJECT
public:
    explicit PartnersPage(std::shared_ptr<pos::Database> database, QWidget* parent = nullptr);
public slots:
    void load();
private:
    bool ensureUnlocked();
    QString selectedPartnerId() const;
    void loadLedger();
    std::shared_ptr<pos::Database> database_;
    QTableWidget* partnersTable_{};
    QTableWidget* ledgerTable_{};
    QPushButton* addBtn_{};
    QPushButton* editBtn_{};
    QPushButton* archiveBtn_{};
    QPushButton* payoutBtn_{};
    QPushButton* refreshBtn_{};
    QDateEdit* fromDate_{};
    QDateEdit* toDate_{};
    QLabel* feedbackLabel_{};
    QLabel* lockedLabel_{};
};
