#pragma once

#include <QWidget>
#include <memory>
#include <QTableWidget>
#include <QPushButton>

namespace pos { class Database; }

class CustomersPage : public QWidget {
    Q_OBJECT
public:
    explicit CustomersPage(std::shared_ptr<pos::Database> database, QWidget* parent = nullptr);
public slots:
    void load();
private:
    void editSelectedCustomer();

    std::shared_ptr<pos::Database> database_;
    QTableWidget* table_{};
    QPushButton* addBtn_{};
    QPushButton* editBtn_{};
    QPushButton* paymentBtn_{};
    QPushButton* refreshBtn_{};
};
