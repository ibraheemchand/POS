#pragma once

#include <QWidget>
#include <memory>
#include <QTableWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

namespace pos { class Database; }

class InventoryPage : public QWidget {
    Q_OBJECT
public:
    explicit InventoryPage(std::shared_ptr<pos::Database> database, QWidget* parent = nullptr);
public slots:
    void load();
private:
    std::shared_ptr<pos::Database> database_;
    QLineEdit* search_{};
    QLabel* val1_{};
    QLabel* val2_{};
    QLabel* val3_{};
    QLabel* val4_{};
    QLabel* resultsCount_{};
    QLabel* paginationText_{};
    QLabel* pageNum_{};
    QTableWidget* table_{};
    QPushButton* editBtn_{};
    QPushButton* archiveBtn_{};
    QPushButton* printLabelBtn_{};
    QPushButton* importCsvBtn_{};
    QPushButton* refreshBtn_{};
    QPushButton* addBtn_{};
};
