#pragma once

#include <QWidget>
#include <memory>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>

namespace pos { class Database; }

class AuditLogPage : public QWidget {
    Q_OBJECT
public:
    explicit AuditLogPage(std::shared_ptr<pos::Database> database, QWidget* parent = nullptr);
    void load();
private:
    std::shared_ptr<pos::Database> database_;
    QLineEdit* filterInput_{};
    QTableWidget* table_{};
    QPushButton* refreshBtn_{};
};
