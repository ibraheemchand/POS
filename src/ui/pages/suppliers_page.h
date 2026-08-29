#pragma once

#include <QWidget>
#include <memory>
#include <QTableWidget>
#include <QPushButton>

namespace pos { class Database; }

class SuppliersPage : public QWidget {
    Q_OBJECT
public:
    explicit SuppliersPage(std::shared_ptr<pos::Database> database, QWidget* parent = nullptr);
public slots:
    void load();
private:
    void loadLedger();

    std::shared_ptr<pos::Database> database_;
    QTableWidget* table_{};
    QTableWidget* ledgerTable_{};
    QPushButton* addBtn_{};
    QPushButton* archiveBtn_{};
    QPushButton* ledgerBtn_{};
    QPushButton* refreshBtn_{};
};
