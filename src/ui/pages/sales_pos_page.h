#pragma once

#include <QWidget>
#include <memory>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>

class DiscountSlider;
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
    void addCartRow(const QString& productId, const QString& productName, const QString& unitName, qint64 quantity, qint64 unitPrice, qint64 discount, bool discountOverrideApproved);
    void updateLineTotal(int row);
    enum class ReceiptAction { None, Print, Pdf };
    void completeSale(ReceiptAction action);
    void savePdfForSale(const QString& saleId); // build + write a PDF, offer to open it
    void holdSale();
    void resumeSale();
    void clearCart();
    void editLineDiscount();
    void loadCourse();
    void recomputeDiscountLimits();       // refresh allowed_/maxDiscount_ and the slider zones
    void commitInvoiceDiscount(qint64 paisa); // apply a discount, prompting for PIN in the red zone
    void syncDiscountWidgets();           // keep slider, number field and labels in step
    QString cartSignature() const;        // detects cart changes that invalidate an override

    std::shared_ptr<pos::Database> database_;
    std::unique_ptr<pos::PosService> posService_;

    QComboBox* customerCombo_{};
    QComboBox* paymentMethodCombo_{};
    QLineEdit* search_{};
    QTableWidget* productsTable_{};
    QSpinBox* quantitySpin_{};
    QPushButton* addToCartBtn_{};
    QPushButton* loadCourseBtn_{};

    QTableWidget* cartTable_{};
    QPushButton* minusBtn_{};
    QPushButton* plusBtn_{};
    QPushButton* removeLineBtn_{};
    QPushButton* lineDiscountBtn_{};
    DiscountSlider* discountSlider_{};
    QDoubleSpinBox* discountAmountSpin_{};
    QPushButton* maxDiscountBtn_{};   // apply the largest no-PIN (green-zone) discount
    QLabel* discountInfoLabel_{};   // "PKR 150 (7.5%)"
    QLabel* allowedLabel_{};        // "Allowed without PIN: up to PKR X"
    qint64 invoiceDiscount_{};      // current invoice discount in paisa
    qint64 allowed_{};              // green-zone end (paisa)
    qint64 maxDiscount_{};          // red-zone end (paisa)
    bool invoiceOverrideApproved_{};
    QString approvedCartSig_;       // cart signature when the override was approved
    bool syncingDiscount_{};        // reentrancy guard for the synced widgets

    QLabel* subtotalValue_{};
    QLabel* discountValue_{};
    QLabel* totalValue_{};
    QDoubleSpinBox* receivedSpin_{};
    QLabel* dueLabel_{};

    QPushButton* savePrintBtn_{};
    QPushButton* savePdfBtn_{};
    QPushButton* saveBtn_{};
    QPushButton* holdBtn_{};
    QPushButton* resumeBtn_{};
    QPushButton* clearBtn_{};
    QPushButton* cancelBtn_{};
    QLabel* feedbackLabel_{};
};
