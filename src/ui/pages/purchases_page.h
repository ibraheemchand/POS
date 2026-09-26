#pragma once

#include <QWidget>
#include <memory>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QDateEdit>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QDate>

namespace pos { class Database; }

class PurchasesPage : public QWidget {
    Q_OBJECT
public:
    explicit PurchasesPage(std::shared_ptr<pos::Database> database, QWidget* parent = nullptr);
    void reloadLists();
public slots:
    void load() { reloadLists(); }
private:
    // Insert a fully-populated cart row (id/unit/tracked stored on the name cell,
    // canonical qty and cost in UserRole). Returns the new row index.
    int addRow(const QString& productId, const QString& name, const QString& unit,
               bool tracked, qint64 quantity, qint64 unitCost, const QString& batch, const QDate& expiry);
    void loadCourse();
    void recomputeTotal();               // totals box + payment UI
    void updatePaymentUi();              // partial fields visibility, remaining, button text
    qint64 cartTotal() const;            // sum of qty x cost (hundredths)
    void refreshRowState(int row); // amber highlight for 0 cost, red for a missing batch
    void onCellChanged(int row, int column); // validate inline edits to qty/cost/batch

    std::shared_ptr<pos::Database> database_;
    QComboBox* supplierCombo_{};
    QComboBox* productCombo_{};
    QSpinBox* quantitySpin_{};
    QDoubleSpinBox* priceSpin_{};
    QLineEdit* batchInput_{};
    QDateEdit* expiryEdit_{};
    QSpinBox* setsSpin_{};
    QTableWidget* cartTable_{};
    QPushButton* addBtn_{};
    QPushButton* loadCourseBtn_{};
    QPushButton* removeBtn_{};
    QPushButton* clearBtn_{};
    QPushButton* saveBtn_{};
    QPushButton* refreshListsBtn_{};
    QLabel* itemsLabel_{};
    QLabel* totalQtyLabel_{};
    QLabel* totalLabel_{};
    QComboBox* paymentCombo_{};
    QLabel* paidLabel_{};
    QDoubleSpinBox* paidSpin_{};
    QLabel* remainingLabel_{};
    bool suppressCellChange_{false}; // guards reentrancy while fixing up an edited cell
};
