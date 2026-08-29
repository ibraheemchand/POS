#pragma once

#include <QWidget>
#include <memory>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>

namespace pos { class Database; class PosService; }

class SalesPosPage : public QWidget {
    Q_OBJECT
public:
    explicit SalesPosPage(std::shared_ptr<pos::Database> database, QWidget* parent = nullptr);
    ~SalesPosPage() override;
public slots:
    void load();
signals:
    void requestNavigation(const QString& pageName);
private:
    void reloadCustomerCombo();
    qint64 computeTotal();
    void refreshDue(qint64 grand);
    void refresh();
    void addToCart();
    void completeSale(bool printReceipt);
    void holdSale();
    void resumeSale();
    void clearCart();

    std::shared_ptr<pos::Database> database_;
    std::unique_ptr<pos::PosService> posService_;

    QComboBox* customerCombo_{};
    QComboBox* paymentMethodCombo_{};
    QLineEdit* search_{};
    QTableWidget* productsTable_{};
    QSpinBox* quantitySpin_{};
    QPushButton* addToCartBtn_{};

    QTableWidget* cartTable_{};
    QPushButton* minusBtn_{};
    QPushButton* plusBtn_{};
    QPushButton* removeLineBtn_{};
    QSpinBox* discountSpin_{};

    QLabel* subtotalValue_{};
    QLabel* discountValue_{};
    QLabel* totalValue_{};
    QSpinBox* receivedSpin_{};
    QLabel* dueLabel_{};

    QPushButton* savePrintBtn_{};
    QPushButton* saveBtn_{};
    QPushButton* holdBtn_{};
    QPushButton* resumeBtn_{};
    QPushButton* clearBtn_{};
    QPushButton* cancelBtn_{};
    QLabel* feedbackLabel_{};
};
