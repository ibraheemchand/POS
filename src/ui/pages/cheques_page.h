#pragma once

#include <QWidget>
#include <memory>
#include <QTableWidget>
#include <QPushButton>

namespace pos { class Database; }

class ChequesPage : public QWidget {
    Q_OBJECT
public:
    explicit ChequesPage(std::shared_ptr<pos::Database> database, QWidget* parent = nullptr);
    void load();
private:
    std::shared_ptr<pos::Database> database_;
    QTableWidget* table_{};
    QPushButton* addBtn_{};
    QPushButton* statusBtn_{};
    QPushButton* refreshBtn_{};
};
