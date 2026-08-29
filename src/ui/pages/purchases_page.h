#pragma once

#include <QWidget>
#include <memory>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QDateEdit>
#include <QTableWidget>
#include <QPushButton>

namespace pos { class Database; }

class PurchasesPage : public QWidget {
    Q_OBJECT
public:
    explicit PurchasesPage(std::shared_ptr<pos::Database> database, QWidget* parent = nullptr);
    void reloadLists();
public slots:
    void load() { reloadLists(); }
private:
    std::shared_ptr<pos::Database> database_;
    QComboBox* supplierCombo_{};
    QComboBox* productCombo_{};
    QSpinBox* quantitySpin_{};
    QSpinBox* priceSpin_{};
    QLineEdit* batchInput_{};
    QDateEdit* expiryEdit_{};
    QTableWidget* cartTable_{};
    QPushButton* addBtn_{};
    QPushButton* removeBtn_{};
    QPushButton* clearBtn_{};
    QPushButton* saveBtn_{};
    QPushButton* refreshListsBtn_{};
};
