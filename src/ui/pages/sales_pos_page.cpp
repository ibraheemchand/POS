#include "ui/pages/sales_pos_page.h"
#include "ui/pages/page_helper.h"
#include "ui/discount_slider.h"
#include "core/database.h"
#include "core/pos_service.h"
#include "core/inventory_service.h"
#include "core/customer_service.h"
#include "core/payment_service.h"
#include "core/suspended_sale_service.h"
#include "core/settings_service.h"
#include "core/thermal_print_service.h"
#include "core/data_change_bus.h"
#include "core/commission_service.h"
#include "core/partner_service.h"
#include "core/bundle_service.h"
#include "core/receipt_service.h"
#include "core/app_paths.h"
#include "ui/receipt_output.h"
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QTimer>

SalesPosPage::~SalesPosPage() = default;

SalesPosPage::SalesPosPage(std::shared_ptr<pos::Database> database, QWidget* parent)
    : QWidget(parent), database_(database), posService_(std::make_unique<pos::PosService>(database)) {
    
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    // Header Panel
    auto* header = new QFrame(this);
    header->setObjectName("panel");
    auto* hl = new QVBoxLayout(header);
    hl->setContentsMargins(14, 8, 14, 8);
    hl->setSpacing(6);

    auto* headTitle = new QLabel("New sale", header);
    headTitle->setObjectName("sectionTitle");
    auto* dateLabel = new QLabel(QDate::currentDate().toString("ddd, dd MMM yyyy"), header);
    dateLabel->setObjectName("muted");

    customerCombo_ = new QComboBox(header);
    customerCombo_->setObjectName("salesCustomer");
    customerCombo_->setAccessibleName("Customer");
    
    auto* customerLabel = new QLabel("Customer (&C):", header);
    customerLabel->setBuddy(customerCombo_);

    paymentMethodCombo_ = new QComboBox(header);
    paymentMethodCombo_->setAccessibleName("Payment type");
    paymentMethodCombo_->addItem("Cash", "cash");
    paymentMethodCombo_->addItem("Credit", "credit");
    paymentMethodCombo_->addItem("Cheque", "cheque");
    paymentMethodCombo_->addItem("Mobile wallet", "mobile_wallet");
    paymentMethodCombo_->addItem("Mixed", "mixed");
    
    auto* paymentLabel = new QLabel("Pay&ment:", header);
    paymentLabel->setBuddy(paymentMethodCombo_);

    auto* headRow = new QHBoxLayout;
    headRow->addWidget(dateLabel);
    headRow->addSpacing(10);
    headRow->addWidget(customerLabel);
    headRow->addWidget(customerCombo_, 1);
    headRow->addSpacing(10);
    headRow->addWidget(paymentLabel);
    headRow->addWidget(paymentMethodCombo_, 1);
    
    hl->addWidget(headTitle);
    hl->addLayout(headRow);
    layout->addWidget(header);

    auto* columns = new QHBoxLayout;
    columns->setSpacing(10);

    // Left Panel: Product Finder
    auto* productPanel = new QFrame(this);
    productPanel->setObjectName("panel");
    auto* productLayout = new QVBoxLayout(productPanel);
    productLayout->setContentsMargins(14, 12, 14, 12);
    productLayout->setSpacing(8);

    auto* productHeading = new QLabel("Product finder", productPanel);
    productHeading->setObjectName("sectionTitle");

    search_ = new QLineEdit(productPanel);
    search_->setObjectName("posSearch");
    search_->setPlaceholderText("Search product, SKU or scan barcode");
    search_->setAccessibleName("Product search or barcode input");
    
    auto* searchLabel = new QLabel("&Find Product:", productPanel);
    searchLabel->setBuddy(search_);

    productsTable_ = new QTableWidget(productPanel);
    productsTable_->setColumnCount(5);
    productsTable_->setHorizontalHeaderLabels({"Product", "SKU", "Stock", "Price", "Unit"});
    productsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    productsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    productsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    productsTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    productsTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    productsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    productsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    productsTable_->setAlternatingRowColors(true);
    productsTable_->verticalHeader()->setVisible(false); // row-number gutter is noise here (and rendered garbled)
    productsTable_->verticalHeader()->setDefaultSectionSize(28);
    productsTable_->setMinimumHeight(190); // ~6 rows visible even at 1280x720
    productsTable_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    quantitySpin_ = new QSpinBox(productPanel);
    quantitySpin_->setRange(1, 1000000);
    quantitySpin_->setPrefix("Qty: ");
    quantitySpin_->setAccessibleName("Item quantity");

    addToCartBtn_ = new QPushButton("Add to cart", productPanel);
    addToCartBtn_->setObjectName("primary");

    loadCourseBtn_ = new QPushButton("Load course…", productPanel);

    auto* addRow = new QHBoxLayout;
    addRow->addWidget(quantitySpin_, 1);
    addRow->addWidget(addToCartBtn_, 2);
    addRow->addWidget(loadCourseBtn_, 2);

    productLayout->setSpacing(6);
    productLayout->addWidget(productHeading);
    productLayout->addWidget(searchLabel);
    productLayout->addWidget(search_);
    productLayout->addWidget(productsTable_, 1);
    productLayout->addLayout(addRow);
    columns->addWidget(productPanel, 3);

    // Right Panel: Current Sale & Checkout
    auto* cartPanel = new QFrame(this);
    cartPanel->setObjectName("panel");
    auto* cartLayout = new QVBoxLayout(cartPanel);
    cartLayout->setContentsMargins(14, 12, 14, 12);
    cartLayout->setSpacing(8);

    auto* cartHeading = new QLabel("Current sale", cartPanel);
    cartHeading->setObjectName("sectionTitle");

    cartTable_ = new QTableWidget(cartPanel);
    cartTable_->setColumnCount(5);
    cartTable_->setHorizontalHeaderLabels({"Product", "Quantity", "Unit price", "Discount", "Line total"});
    cartTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    cartTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    cartTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    cartTable_->setAlternatingRowColors(true);
    cartTable_->verticalHeader()->setVisible(false);
    cartTable_->verticalHeader()->setDefaultSectionSize(28);
    // Never collapse below 5 rows (5 x 28px): the cart must stay usable when tight.
    cartTable_->setMinimumHeight(140);
    cartTable_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    minusBtn_ = new QPushButton("−1", cartPanel);
    plusBtn_ = new QPushButton("+1", cartPanel);
    removeLineBtn_ = new QPushButton("Remove line", cartPanel);
    removeLineBtn_->setObjectName("danger");
    lineDiscountBtn_ = new QPushButton("Discount line…", cartPanel);

    auto* rowActions = new QHBoxLayout;
    rowActions->addWidget(minusBtn_);
    rowActions->addWidget(plusBtn_);
    rowActions->addWidget(lineDiscountBtn_);
    rowActions->addWidget(removeLineBtn_);
    rowActions->addStretch();

    // Invoice discount: a slider with a synced number field. Green zone = free,
    // red zone = needs a manager PIN. Amounts only — no commission internals shown.
    discountSlider_ = new DiscountSlider(cartPanel);
    discountSlider_->setAccessibleName("Invoice discount slider");
    discountAmountSpin_ = new QDoubleSpinBox(cartPanel);
    discountAmountSpin_->setRange(0, 100000000);
    discountAmountSpin_->setDecimals(2);
    discountAmountSpin_->setPrefix("PKR ");
    discountAmountSpin_->setAccessibleName("Invoice discount in rupees");
    discountAmountSpin_->setMaximumWidth(150);
    maxDiscountBtn_ = new QPushButton("Max", cartPanel);
    maxDiscountBtn_->setToolTip("Apply the largest discount allowed without a manager PIN");
    maxDiscountBtn_->setAccessibleName("Apply maximum allowed discount");
    auto* discountRow = new QHBoxLayout;
    discountRow->addWidget(discountSlider_, 1);
    discountRow->addWidget(discountAmountSpin_);
    discountRow->addWidget(maxDiscountBtn_);

    discountInfoLabel_ = new QLabel("PKR 0 (0%)", cartPanel);
    discountInfoLabel_->setObjectName("sumValue");
    allowedLabel_ = new QLabel("Allowed without PIN: up to PKR 0", cartPanel);
    allowedLabel_->setObjectName("muted");
    auto* discountInfoRow = new QHBoxLayout;
    discountInfoRow->addWidget(discountInfoLabel_);
    discountInfoRow->addStretch();
    discountInfoRow->addWidget(allowedLabel_);


    auto* summary = new QFrame(cartPanel);
    summary->setObjectName("summaryBox");
    auto* sl = new QVBoxLayout(summary);
    sl->setContentsMargins(10, 5, 10, 5);
    sl->setSpacing(2);

    auto* subtotalLabel = new QLabel("Subtotal", summary);
    subtotalLabel->setObjectName("sumLabel");
    subtotalValue_ = new QLabel("PKR 0.00", summary);
    subtotalValue_->setObjectName("sumValue");

    auto* summaryDiscountLabel = new QLabel("Discount", summary);
    summaryDiscountLabel->setObjectName("sumLabel");
    discountValue_ = new QLabel("PKR 0.00", summary);
    discountValue_->setObjectName("sumValue");

    auto* subRow = new QHBoxLayout;
    subRow->addWidget(subtotalLabel);
    subRow->addStretch();
    subRow->addWidget(subtotalValue_);

    // The invoice discount is shown by discountInfoLabel_ next to the slider, so the
    // summary's own Discount line is redundant — hidden to keep the panel compact at
    // 1280x720. discountValue_ stays alive (computeTotal still updates it).
    summaryDiscountLabel->setVisible(false);
    discountValue_->setVisible(false);

    totalValue_ = new QLabel("PKR 0.00", summary);
    totalValue_->setObjectName("posTotal");
    totalValue_->setAlignment(Qt::AlignRight);

    sl->addLayout(subRow);
    sl->addSpacing(2);
    sl->addWidget(totalValue_);

    receivedSpin_ = new QDoubleSpinBox(cartPanel);
    receivedSpin_->setRange(0, 100000000);
    receivedSpin_->setDecimals(2);
    receivedSpin_->setPrefix("Amount received (PKR): ");
    receivedSpin_->setAccessibleName("Amount received in rupees");
    

    dueLabel_ = new QLabel("Change: PKR 0.00", cartPanel);
    dueLabel_->setObjectName("muted");

    savePrintBtn_ = new QPushButton("Save && &Print", cartPanel); // "&&" renders a literal & ; &P keeps Alt+P
    savePrintBtn_->setObjectName("primary");
    savePrintBtn_->setMinimumHeight(32);

    savePdfBtn_ = new QPushButton("Save as P&DF", cartPanel);
    saveBtn_ = new QPushButton("Sa&ve", cartPanel);
    holdBtn_ = new QPushButton("&Hold", cartPanel);
    resumeBtn_ = new QPushButton("R&esume", cartPanel);
    clearBtn_ = new QPushButton("C&lear", cartPanel);
    clearBtn_->setObjectName("danger");
    cancelBtn_ = new QPushButton("Cancel (&X)", cartPanel);

    auto* primaryRow = new QHBoxLayout;
    primaryRow->addWidget(savePrintBtn_, 3);
    primaryRow->addWidget(savePdfBtn_, 2);
    primaryRow->addWidget(saveBtn_, 2);

    auto* secondaryRow = new QHBoxLayout;
    secondaryRow->addWidget(holdBtn_, 2);
    secondaryRow->addWidget(resumeBtn_, 2);
    secondaryRow->addWidget(clearBtn_, 2);
    secondaryRow->addWidget(cancelBtn_, 2);

    feedbackLabel_ = new QLabel("", cartPanel);
    feedbackLabel_->setObjectName("muted");
    feedbackLabel_->setWordWrap(true);

    cartLayout->setSpacing(4);
    cartLayout->addWidget(cartHeading);
    cartLayout->addWidget(cartTable_, 1);      // stretches — the cart is the flexible area
    cartLayout->addLayout(rowActions);
    cartLayout->addLayout(discountRow);
    cartLayout->addLayout(discountInfoRow);
    cartLayout->addWidget(summary);
    cartLayout->addWidget(receivedSpin_);      // prefix says "Amount received (PKR)"
    cartLayout->addWidget(dueLabel_);
    cartLayout->addLayout(primaryRow);
    cartLayout->addLayout(secondaryRow);
    cartLayout->addWidget(feedbackLabel_);
    columns->addWidget(cartPanel, 2);
    layout->addLayout(columns, 1);

    // Initial setups
    reloadCustomerCombo();
    QTimer::singleShot(0, this, [this] { load(); });

    // Auto update wiring
    connect(&pos::DataChangeBus::instance(), &pos::DataChangeBus::inventoryChanged, this, &SalesPosPage::load);
    connect(&pos::DataChangeBus::instance(), &pos::DataChangeBus::customersChanged, this, [this] {
        reloadCustomerCombo();
        load();
    });

    // Connections
    connect(search_, &QLineEdit::textChanged, this, [this] { load(); });
    connect(search_, &QLineEdit::returnPressed, this, [this] {
        if (productsTable_->rowCount() > 0) {
            productsTable_->setCurrentCell(0, 0);
            addToCart();
        }
    });
    connect(productsTable_, &QTableWidget::itemDoubleClicked, this, [this] { addToCart(); });
    connect(addToCartBtn_, &QPushButton::clicked, this, &SalesPosPage::addToCart);

    connect(plusBtn_, &QPushButton::clicked, this, [this] {
        const int row = cartTable_->currentRow();
        if (row < 0) return;
        const auto id = cartTable_->item(row, 0)->data(Qt::UserRole).toString();

        pos::InventoryService inv(database_);
        const auto stock = inv.getStock(id);
        const auto newQuantity = cartTable_->item(row, 1)->text().toLongLong() + 1;
        if (newQuantity > stock) {
            feedbackLabel_->setText(QString("Not enough stock — available: %1").arg(stock));
            return;
        }
        cartTable_->item(row, 1)->setText(QString::number(newQuantity));
        updateLineTotal(row);
        feedbackLabel_->clear();
        refresh();
    });

    connect(minusBtn_, &QPushButton::clicked, this, [this] {
        const int row = cartTable_->currentRow();
        if (row < 0) return;
        const auto current = cartTable_->item(row, 1)->text().toLongLong();
        if (current <= 1) {
            cartTable_->removeRow(row);
            refresh();
            return;
        }
        cartTable_->item(row, 1)->setText(QString::number(current - 1));
        updateLineTotal(row);
        refresh();
    });

    connect(removeLineBtn_, &QPushButton::clicked, this, [this] {
        const int row = cartTable_->currentRow();
        if (row >= 0) {
            cartTable_->removeRow(row);
            refresh();
        }
    });

    connect(lineDiscountBtn_, &QPushButton::clicked, this, &SalesPosPage::editLineDiscount);
    connect(cartTable_, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
        if (column == 3) {
            cartTable_->setCurrentCell(row, column);
            editLineDiscount();
        }
    });
    connect(loadCourseBtn_, &QPushButton::clicked, this, &SalesPosPage::loadCourse);

    // Dragging the slider: preview the amount live (no PIN yet). The PIN prompt for
    // the red zone happens on release, so the salesman can drag freely.
    connect(discountSlider_, &QSlider::valueChanged, this, [this](int paisa) {
        if (syncingDiscount_) return;
        invoiceDiscount_ = paisa;
        if (paisa <= allowed_) invoiceOverrideApproved_ = false;
        syncDiscountWidgets();
        const auto grand = computeTotal();
        if (paymentMethodCombo_->currentData().toString() != "credit") receivedSpin_->setValue(grand / 100.0);
        refreshDue(grand);
    });
    connect(discountSlider_, &QSlider::sliderReleased, this, [this] { commitInvoiceDiscount(discountSlider_->value()); });
    // Typing an amount: commit it (with PIN if in the red zone) when editing ends.
    connect(discountAmountSpin_, &QDoubleSpinBox::editingFinished, this, [this] {
        if (syncingDiscount_) return;
        commitInvoiceDiscount(pos::roundMoney(discountAmountSpin_->value() * 100));
    });
    // Apply the largest no-PIN discount (top of the green zone).
    connect(maxDiscountBtn_, &QPushButton::clicked, this, [this] { commitInvoiceDiscount(allowed_); });

    connect(paymentMethodCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        const auto grand = computeTotal();
        if (paymentMethodCombo_->currentData().toString() != "credit") {
            receivedSpin_->setValue(grand / 100.0);
        }
        refreshDue(grand);
    });

    connect(receivedSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        refreshDue(computeTotal());
    });

    connect(saveBtn_, &QPushButton::clicked, this, [this] { completeSale(ReceiptAction::None); });
    connect(savePrintBtn_, &QPushButton::clicked, this, [this] { completeSale(ReceiptAction::Print); });
    connect(savePdfBtn_, &QPushButton::clicked, this, [this] { completeSale(ReceiptAction::Pdf); });
    connect(holdBtn_, &QPushButton::clicked, this, &SalesPosPage::holdSale);
    connect(resumeBtn_, &QPushButton::clicked, this, &SalesPosPage::resumeSale);
    connect(clearBtn_, &QPushButton::clicked, this, &SalesPosPage::clearCart);
    connect(cancelBtn_, &QPushButton::clicked, this, [this] {
        if (cartTable_->rowCount() && QMessageBox::question(this, "Cancel sale", "Abandon this sale and return to the dashboard?") != QMessageBox::Yes) return;
        cartTable_->setRowCount(0);
        invoiceDiscount_ = 0; invoiceOverrideApproved_ = false; approvedCartSig_.clear();
        refresh();
        emit requestNavigation("Dashboard");
    });

    // Data Change Bus listeners
    connect(&pos::DataChangeBus::instance(), &pos::DataChangeBus::inventoryChanged, this, &SalesPosPage::load);
    connect(&pos::DataChangeBus::instance(), &pos::DataChangeBus::customersChanged, this, &SalesPosPage::reloadCustomerCombo);

    // Set tab order
    setTabOrder(customerCombo_, paymentMethodCombo_);
    setTabOrder(paymentMethodCombo_, search_);
    setTabOrder(search_, productsTable_);
    setTabOrder(productsTable_, quantitySpin_);
    setTabOrder(quantitySpin_, addToCartBtn_);
    setTabOrder(addToCartBtn_, loadCourseBtn_);
    setTabOrder(loadCourseBtn_, cartTable_);
    setTabOrder(cartTable_, lineDiscountBtn_);
    setTabOrder(lineDiscountBtn_, discountAmountSpin_);
    setTabOrder(discountAmountSpin_, discountSlider_);
    setTabOrder(discountSlider_, maxDiscountBtn_);
    setTabOrder(maxDiscountBtn_, receivedSpin_);
    setTabOrder(receivedSpin_, savePrintBtn_);
    setTabOrder(savePrintBtn_, savePdfBtn_);
    setTabOrder(savePdfBtn_, saveBtn_);
    setTabOrder(saveBtn_, holdBtn_);
    setTabOrder(holdBtn_, resumeBtn_);
    setTabOrder(resumeBtn_, clearBtn_);
    setTabOrder(clearBtn_, cancelBtn_);

    syncDiscountWidgets(); // initial labels/slider state (empty cart)
}

void SalesPosPage::reloadCustomerCombo() {
    customerCombo_->clear();
    customerCombo_->addItem("— Walk-in customer —", QString());
    try {
        const auto list = pos::CustomerService(database_).listActive();
        for (const auto& item : list) {
            customerCombo_->addItem(item.name, item.id);
        }
    } catch (...) {}
}

void SalesPosPage::load() {
    productsTable_->setRowCount(0);
    try {
        pos::InventoryService inv(database_);
        const auto term = search_->text().trimmed();
        const auto list = inv.searchPOSProducts(term);
        for (const auto& item : list) {
            const int row = productsTable_->rowCount();
            productsTable_->insertRow(row);
            
            auto* nameItem = new QTableWidgetItem(item.name);
            nameItem->setData(Qt::UserRole, item.id);
            productsTable_->setItem(row, 0, nameItem);
            
            productsTable_->setItem(row, 1, new QTableWidgetItem(item.sku));
            productsTable_->setItem(row, 2, new QTableWidgetItem(QString::number(item.stock)));
            
            auto* priceItem = new QTableWidgetItem("PKR " + pos::formatMoney(item.retailPrice));
            priceItem->setData(Qt::UserRole + 2, item.retailPrice);
            productsTable_->setItem(row, 3, priceItem);
            
            productsTable_->setItem(row, 4, new QTableWidgetItem(item.baseUnit));
        }
    } catch (...) {}
}

qint64 SalesPosPage::computeTotal() {
    qint64 subtotal{};
    for (int row = 0; row < cartTable_->rowCount(); ++row) {
        subtotal += cartTable_->item(row, 4)->data(Qt::UserRole).toLongLong();
    }
    const qint64 disc = invoiceDiscount_;
    const qint64 grand = subtotal - disc;
    subtotalValue_->setText("PKR " + pos::formatMoney(subtotal));
    discountValue_->setText("PKR " + pos::formatMoney(disc));
    totalValue_->setText("PKR " + pos::formatMoney(grand));
    return grand;
}

void SalesPosPage::refreshDue(qint64 grand) {
    if (paymentMethodCombo_->currentData().toString() == "credit") {
        receivedSpin_->setEnabled(false);
        dueLabel_->setText(QString("Balance due: PKR %1").arg(pos::formatMoney(grand)));
    } else {
        receivedSpin_->setEnabled(true);
        const qint64 paid = pos::roundMoney(receivedSpin_->value() * 100);
        if (paid >= grand) {
            dueLabel_->setText(QString("Change: PKR %1").arg(pos::formatMoney(paid - grand)));
        } else {
            dueLabel_->setText(QString("Balance due: PKR %1").arg(pos::formatMoney(grand - paid)));
        }
    }
}

void SalesPosPage::refresh() {
    recomputeDiscountLimits();
    const auto grand = computeTotal();
    if (paymentMethodCombo_->currentData().toString() != "credit") {
        receivedSpin_->setValue(grand / 100.0);
    }
    refreshDue(grand);
}

QString SalesPosPage::cartSignature() const {
    QString sig;
    for (int r = 0; r < cartTable_->rowCount(); ++r) {
        sig += cartTable_->item(r, 0)->data(Qt::UserRole).toString() + ':'
             + cartTable_->item(r, 1)->text() + ':'
             + QString::number(cartTable_->item(r, 2)->data(Qt::UserRole + 2).toLongLong()) + ':'
             + QString::number(cartTable_->item(r, 3)->data(Qt::UserRole).toLongLong()) + ';';
    }
    return sig;
}

void SalesPosPage::recomputeDiscountLimits() {
    QList<pos::SaleLine> lines;
    for (int r = 0; r < cartTable_->rowCount(); ++r) {
        pos::SaleLine line;
        line.productId = cartTable_->item(r, 0)->data(Qt::UserRole).toString();
        line.courseId = cartTable_->item(r, 0)->data(Qt::UserRole + 2).toString();
        line.quantity = cartTable_->item(r, 1)->text().toLongLong();
        line.unitPrice = cartTable_->item(r, 2)->data(Qt::UserRole + 2).toLongLong();
        line.discount = cartTable_->item(r, 3)->data(Qt::UserRole).toLongLong();
        lines.append(line);
    }
    try {
        const auto limits = pos::CommissionService(database_).cartDiscountLimits(lines);
        allowed_ = limits.allowed;
        maxDiscount_ = limits.max;
    } catch (...) { allowed_ = 0; maxDiscount_ = 0; }

    // An override is only valid for the cart it was approved for.
    const auto sig = cartSignature();
    if (invoiceOverrideApproved_ && sig != approvedCartSig_) invoiceOverrideApproved_ = false;
    if (invoiceDiscount_ > maxDiscount_) invoiceDiscount_ = maxDiscount_;
    // Cart change pushed the discount into the red zone → ask for the PIN again.
    if (invoiceDiscount_ > allowed_ && !invoiceOverrideApproved_) {
        if (pos::authorizeSensitiveAction(this, database_, "apply an invoice discount beyond the allowed margin")) {
            invoiceOverrideApproved_ = true;
            approvedCartSig_ = sig;
        } else {
            invoiceDiscount_ = allowed_;
        }
    }
    syncDiscountWidgets();
}

void SalesPosPage::commitInvoiceDiscount(qint64 paisa) {
    if (paisa < 0) paisa = 0;
    if (paisa > maxDiscount_) paisa = maxDiscount_;
    if (paisa > allowed_) {
        if (pos::authorizeSensitiveAction(this, database_, "apply an invoice discount beyond the allowed margin")) {
            invoiceOverrideApproved_ = true;
            approvedCartSig_ = cartSignature();
        } else {
            paisa = allowed_;
            invoiceOverrideApproved_ = false;
        }
    } else {
        invoiceOverrideApproved_ = false;
    }
    invoiceDiscount_ = paisa;
    syncDiscountWidgets();
    const auto grand = computeTotal();
    if (paymentMethodCombo_->currentData().toString() != "credit") receivedSpin_->setValue(grand / 100.0);
    refreshDue(grand);
}

void SalesPosPage::syncDiscountWidgets() {
    syncingDiscount_ = true;
    discountSlider_->setMaximum(maxDiscount_ > 0 ? static_cast<int>(maxDiscount_) : 1);
    discountSlider_->setEnabled(maxDiscount_ > 0);
    discountSlider_->setAllowed(allowed_);
    discountSlider_->setValue(static_cast<int>(invoiceDiscount_));
    discountAmountSpin_->setMaximum(maxDiscount_ / 100.0);
    discountAmountSpin_->setValue(invoiceDiscount_ / 100.0);
    maxDiscountBtn_->setEnabled(allowed_ > 0);
    syncingDiscount_ = false;

    qint64 subtotal = 0;
    for (int r = 0; r < cartTable_->rowCount(); ++r) subtotal += cartTable_->item(r, 4)->data(Qt::UserRole).toLongLong();
    const double pct = subtotal > 0 ? (100.0 * invoiceDiscount_ / subtotal) : 0.0;
    discountInfoLabel_->setText(QString("PKR %1 (%2%)").arg(pos::formatMoney(invoiceDiscount_)).arg(pct, 0, 'f', 1));
    allowedLabel_->setText("Allowed without PIN: up to PKR " + pos::formatMoney(allowed_));
}

void SalesPosPage::addToCart() {
    const int row = productsTable_->currentRow();
    if (row < 0) return;
    const auto id = productsTable_->item(row, 0)->data(Qt::UserRole).toString();
    const auto count = quantitySpin_->value();
    const auto price = productsTable_->item(row, 3)->data(Qt::UserRole + 2).toLongLong();
    const auto stock = productsTable_->item(row, 2)->text().toLongLong();

    for (int current = 0; current < cartTable_->rowCount(); ++current) {
        if (cartTable_->item(current, 0)->data(Qt::UserRole).toString() == id) {
            const auto newQuantity = cartTable_->item(current, 1)->text().toLongLong() + count;
            if (newQuantity > stock) {
                feedbackLabel_->setText(QString("Not enough stock — available: %1").arg(stock));
                return;
            }
            cartTable_->item(current, 1)->setText(QString::number(newQuantity));
            updateLineTotal(current);
            feedbackLabel_->clear();
            refresh();
            return;
        }
    }

    if (count > stock) {
        feedbackLabel_->setText(QString("Not enough stock — available: %1").arg(stock));
        return;
    }

    addCartRow(id, productsTable_->item(row, 0)->text(), productsTable_->item(row, 4)->text(), count, price, 0, false);
    feedbackLabel_->clear();
    refresh();
}

void SalesPosPage::addCartRow(const QString& productId, const QString& productName, const QString& unitName, qint64 quantity, qint64 unitPrice, qint64 discount, bool discountOverrideApproved) {
    const int target = cartTable_->rowCount();
    cartTable_->insertRow(target);

    auto* name = new QTableWidgetItem(productName);
    name->setData(Qt::UserRole, productId);
    name->setData(Qt::UserRole + 1, unitName);
    cartTable_->setItem(target, 0, name);

    cartTable_->setItem(target, 1, new QTableWidgetItem(QString::number(quantity)));

    auto* priceItem = new QTableWidgetItem(pos::formatMoney(unitPrice));
    priceItem->setData(Qt::UserRole + 2, unitPrice);
    cartTable_->setItem(target, 2, priceItem);

    auto* discountItem = new QTableWidgetItem(pos::formatMoney(discount));
    discountItem->setData(Qt::UserRole, discount);
    discountItem->setData(Qt::UserRole + 1, discountOverrideApproved);
    cartTable_->setItem(target, 3, discountItem);

    auto* amountItem = new QTableWidgetItem;
    cartTable_->setItem(target, 4, amountItem);

    updateLineTotal(target);
}

void SalesPosPage::updateLineTotal(int row) {
    if (row < 0 || row >= cartTable_->rowCount()) return;
    const auto quantity = cartTable_->item(row, 1)->text().toLongLong();
    const auto price = cartTable_->item(row, 2)->data(Qt::UserRole + 2).toLongLong();
    auto* discountItem = cartTable_->item(row, 3);
    auto discount = discountItem->data(Qt::UserRole).toLongLong();
    const auto retail = quantity * price;
    if (discount > retail) {
        // Quantity dropped below what the existing discount assumed; clamp so the
        // line total can never go negative. PosService still re-validates the cap.
        discount = retail;
        discountItem->setData(Qt::UserRole, discount);
    }
    discountItem->setText(pos::formatMoney(discount));
    auto* amountItem = cartTable_->item(row, 4);
    const auto net = retail - discount;
    amountItem->setText(pos::formatMoney(net));
    amountItem->setData(Qt::UserRole, net);
}

void SalesPosPage::editLineDiscount() {
    const int row = cartTable_->currentRow();
    if (row < 0) {
        QMessageBox::information(this, "Discount line", "Select a cart line first.");
        return;
    }
    const auto quantity = cartTable_->item(row, 1)->text().toLongLong();
    const auto price = cartTable_->item(row, 2)->data(Qt::UserRole + 2).toLongLong();
    const auto retail = quantity * price;
    auto* discountItem = cartTable_->item(row, 3);
    const auto currentDiscount = discountItem->data(Qt::UserRole).toLongLong();

    // Cap is item-specific: (this item's total% - partner% - owner min%) of the
    // gross line. A course-loaded line uses the course's settings; anything with
    // no commission configured gets a cap of 0 (any discount needs a manager PIN).
    pos::Money cap = 0;
    try {
        const auto productId = cartTable_->item(row, 0)->data(Qt::UserRole).toString();
        const auto courseId = cartTable_->item(row, 0)->data(Qt::UserRole + 2).toString();
        pos::PartnerService partners(database_);
        const auto cfg = courseId.isEmpty() ? partners.resolveBookConfig(productId) : partners.resolveCourseConfig(courseId);
        cap = pos::CommissionService(database_).flexibleCap(retail, cfg.totalBp, cfg.partnerBp);
    } catch (...) {}

    bool ok = false;
    QMessageBox::information(this, "Discount line", QString("Up to %1 may be discounted on this line without a manager override.").arg(pos::formatMoney(cap)));
    const auto discount = pos::askMoney(this, "Discount line", "Discount for this line", currentDiscount, &ok);
    if (!ok) return;
    if (discount > retail) { QMessageBox::warning(this, "Discount line", "The discount cannot exceed the line amount."); return; }

    bool overrideApproved = false;
    if (discount > cap) {
        if (!pos::authorizeSensitiveAction(this, database_, "give a discount beyond the flexible commission margin")) return;
        overrideApproved = true;
    }

    discountItem->setData(Qt::UserRole, static_cast<qint64>(discount));
    discountItem->setData(Qt::UserRole + 1, overrideApproved);
    updateLineTotal(row);
    refresh();
}

void SalesPosPage::loadCourse() {
    try {
        const auto bundles = pos::BundleService(database_).listBundles();
        if (bundles.isEmpty()) {
            QMessageBox::information(this, "Load course", "No courses are set up yet. Add one from the Courses page.");
            return;
        }
        QStringList choices;
        for (const auto& bundle : bundles) {
            choices.append(QString("%1 — %2 (%3 books)").arg(bundle.name, bundle.gradeLabel).arg(bundle.itemCount));
        }
        bool ok = false;
        const auto selected = QInputDialog::getItem(this, "Load course", "Course:", choices, 0, false, &ok);
        if (!ok) return;
        const auto bundle = bundles.at(choices.indexOf(selected));

        const auto items = pos::BundleService(database_).resolveItems(bundle.id);
        QStringList outOfStock;
        int added = 0;
        for (const auto& item : items) {
            if (item.stock < item.quantity) {
                outOfStock.append(QString("%1 (need %2, have %3)").arg(item.productName).arg(item.quantity).arg(item.stock));
                continue;
            }
            bool merged = false;
            for (int row = 0; row < cartTable_->rowCount(); ++row) {
                // Only merge into a line already tagged with THIS course, so a
                // manual line of the same book stays a standalone (book-settings) line.
                if (cartTable_->item(row, 0)->data(Qt::UserRole).toString() == item.productId
                    && cartTable_->item(row, 0)->data(Qt::UserRole + 2).toString() == bundle.id) {
                    const auto newQuantity = cartTable_->item(row, 1)->text().toLongLong() + item.quantity;
                    cartTable_->item(row, 1)->setText(QString::number(newQuantity));
                    updateLineTotal(row);
                    merged = true;
                    break;
                }
            }
            if (!merged) {
                addCartRow(item.productId, item.productName, item.baseUnit, item.quantity, item.retailPrice, 0, false);
                // Tag the new line with its course so checkout applies the course's
                // commission (grouped once), not the book's own settings.
                cartTable_->item(cartTable_->rowCount() - 1, 0)->setData(Qt::UserRole + 2, bundle.id);
            }
            ++added;
        }
        refresh();
        if (!outOfStock.isEmpty()) {
            feedbackLabel_->setText(QString("Loaded %1 book(s) from %2. Skipped (out of stock): %3").arg(added).arg(bundle.name, outOfStock.join(", ")));
        } else {
            feedbackLabel_->setText(QString("Loaded %1 book(s) from %2. Review the cart before checkout.").arg(added).arg(bundle.name));
        }
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Could not load course", error.what());
    }
}

void SalesPosPage::completeSale(ReceiptAction action) {
    if (!cartTable_->rowCount()) {
        feedbackLabel_->setText("Add at least one product to the cart.");
        return;
    }
    try {
        pos::SaleRequest request;
        request.paymentMethod = paymentMethodCombo_->currentData().toString();
        request.customerId = customerCombo_->currentData().toString();
        request.invoiceDiscount = invoiceDiscount_;
        request.invoiceDiscountOverrideApproved = invoiceOverrideApproved_;

        qint64 subtotal{};
        for (int row = 0; row < cartTable_->rowCount(); ++row) {
            const auto count = cartTable_->item(row, 1)->text().toLongLong();
            const auto price = cartTable_->item(row, 2)->data(Qt::UserRole + 2).toLongLong();
            const auto discount = cartTable_->item(row, 3)->data(Qt::UserRole).toLongLong();
            const auto discountOverrideApproved = cartTable_->item(row, 3)->data(Qt::UserRole + 1).toBool();
            subtotal += count * price - discount;
            request.lines.append({
                cartTable_->item(row, 0)->data(Qt::UserRole).toString(),
                {},
                count,
                price,
                discount,
                cartTable_->item(row, 0)->data(Qt::UserRole + 1).toString(),
                discountOverrideApproved,
                cartTable_->item(row, 0)->data(Qt::UserRole + 2).toString()
            });
        }

        if (request.invoiceDiscount > subtotal) {
            QMessageBox::warning(this, "Invalid discount", "Discount cannot exceed the cart subtotal.");
            return;
        }

        request.paidAmount = request.paymentMethod == "credit" ? 0 : pos::roundMoney(receivedSpin_->value() * 100);
        if (request.paidAmount > subtotal - request.invoiceDiscount) {
            request.paidAmount = subtotal - request.invoiceDiscount;
        }

        if (request.paymentMethod == "mixed") {
            const auto maxTender = request.paidAmount;
            bool ok = false;
            const auto cashAmt = pos::askMoney(this, "Mixed payment", QString("Cash amount (total due %1)").arg(pos::formatMoney(maxTender)), 0, &ok);
            if (!ok) return;
            const auto chequeAmt = pos::askMoney(this, "Mixed payment", QString("Cheque amount (remaining %1)").arg(pos::formatMoney(maxTender - cashAmt)), 0, &ok);
            if (!ok) return;
            const auto mobileAmt = maxTender - cashAmt - chequeAmt;
            if (cashAmt + chequeAmt + mobileAmt != maxTender) {
                QMessageBox::warning(this, "Invalid payment", "Tender amounts must equal the sale total.");
                return;
            }
            if (cashAmt > 0) request.tenders.append({"cash", cashAmt});
            if (chequeAmt > 0) request.tenders.append({"cheque", chequeAmt});
            if (mobileAmt > 0) request.tenders.append({"mobile_wallet", mobileAmt});
        }

        const auto due = subtotal - request.invoiceDiscount - request.paidAmount;
        if (due > 0 && request.customerId.isEmpty()) {
            QMessageBox::warning(this, "Customer required", "An unpaid balance needs a customer. Select one or increase the amount received.");
            return;
        }

        const auto sale = posService_->completeSale(request);
        cartTable_->setRowCount(0);
        invoiceDiscount_ = 0; invoiceOverrideApproved_ = false; approvedCartSig_.clear();
        receivedSpin_->setValue(0);
        paymentMethodCombo_->setCurrentIndex(0);
        customerCombo_->setCurrentIndex(0);
        refresh();
        load();

        if (action == ReceiptAction::Pdf) {
            savePdfForSale(sale.saleId);
        } else if (action == ReceiptAction::Print) {
            try {
                const auto status = pos::ui::deliverReceipt(database_, pos::ReceiptService(database_).buildFromSale(sale.saleId));
                feedbackLabel_->setText(QString("Invoice %1 saved. %2").arg(sale.invoiceNo, status));
            } catch (const std::exception& e) {
                feedbackLabel_->setText(QString("Invoice %1 saved but the receipt could not be printed: %2").arg(sale.invoiceNo, e.what()));
            }
        } else {
            feedbackLabel_->setText(QString("Invoice %1 saved.").arg(sale.invoiceNo));
        }
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Sale failed", error.what());
    }
}

void SalesPosPage::savePdfForSale(const QString& saleId) {
    try {
        const auto data = pos::ReceiptService(database_).buildFromSale(saleId);
        const auto dir = pos::paths::receiptsDir();
        const auto path = QString("%1/Receipt_%2.pdf").arg(dir, QString(data.invoiceNo).replace('/', '-'));
        pos::ReceiptService::renderPdf(data, path);
        QMessageBox box(QMessageBox::Information, "Receipt saved as PDF",
                        QString("Saved to:\n%1").arg(QDir::toNativeSeparators(path)), QMessageBox::Ok, this);
        auto* openBtn = box.addButton("Open", QMessageBox::AcceptRole);
        auto* folderBtn = box.addButton("Open receipts folder", QMessageBox::ActionRole);
        box.exec();
        if (box.clickedButton() == openBtn) QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        else if (box.clickedButton() == folderBtn) QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        feedbackLabel_->setText(QString("Invoice %1 saved. PDF: %2").arg(data.invoiceNo, QDir::toNativeSeparators(path)));
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Could not save PDF", e.what());
    }
}

void SalesPosPage::holdSale() {
    if (!cartTable_->rowCount()) return;
    try {
        QList<pos::SuspendedLine> lines;
        for (int row = 0; row < cartTable_->rowCount(); ++row) {
            lines.append({
                cartTable_->item(row, 0)->data(Qt::UserRole).toString(),
                cartTable_->item(row, 1)->text().toLongLong(),
                cartTable_->item(row, 2)->data(Qt::UserRole + 2).toLongLong(),
                cartTable_->item(row, 0)->data(Qt::UserRole + 1).toString()
            });
        }
        pos::SuspendedSaleService(database_).save(lines);
        cartTable_->setRowCount(0);
        invoiceDiscount_ = 0; invoiceOverrideApproved_ = false; approvedCartSig_.clear();
        refresh();
        feedbackLabel_->setText("Sale held and saved locally. Use Resume to bring it back.");
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Could not hold sale", error.what());
    }
}

void SalesPosPage::resumeSale() {
    try {
        const auto saved = pos::SuspendedSaleService(database_).list();
        if (saved.isEmpty()) {
            feedbackLabel_->setText("No held sales are available.");
            return;
        }
        QStringList choices;
        for (const auto& sale : saved) {
            choices.append(sale.createdAt + " (" + sale.id.left(8) + ")");
        }
        bool ok = false;
        const auto selected = QInputDialog::getItem(this, "Resume sale", "Held cart:", choices, 0, false, &ok);
        if (!ok) return;
        
        const auto sale = pos::SuspendedSaleService(database_).load(saved.at(choices.indexOf(selected)).id);
        cartTable_->setRowCount(0);

        for (const auto& line : sale.lines) {
            auto product = database_->prepare("SELECT name, stock_quantity FROM products WHERE id=? AND is_deleted=0");
            product.bind(1, line.productId);
            if (!product.stepRow() || product.integer(1) < line.quantity) {
                throw pos::DatabaseError("a held product is unavailable or out of stock");
            }
            addCartRow(line.productId, product.text(0), line.unit, line.quantity, line.unitPrice, 0, false);
        }
        pos::SuspendedSaleService(database_).remove(sale.id);
        refresh();
        feedbackLabel_->setText("Held sale restored to the cart.");
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Could not resume sale", error.what());
    }
}

void SalesPosPage::clearCart() {
    if (!cartTable_->rowCount()) return;
    if (QMessageBox::question(this, "Clear sale", "Clear the current cart?") == QMessageBox::Yes) {
        cartTable_->setRowCount(0);
        invoiceDiscount_ = 0; invoiceOverrideApproved_ = false; approvedCartSig_.clear();
        refresh();
        feedbackLabel_->setText("Cart cleared.");
    }
}
