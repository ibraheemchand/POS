#include "ui/pages/sales_pos_page.h"
#include "ui/pages/page_helper.h"
#include "core/database.h"
#include "core/pos_service.h"
#include "core/inventory_service.h"
#include "core/customer_service.h"
#include "core/payment_service.h"
#include "core/suspended_sale_service.h"
#include "core/settings_service.h"
#include "core/thermal_print_service.h"
#include "core/data_change_bus.h"
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
    layout->setSpacing(10);

    // Header Panel
    auto* header = new QFrame(this);
    header->setObjectName("panel");
    auto* hl = new QVBoxLayout(header);
    hl->setContentsMargins(16, 10, 16, 10);
    hl->setSpacing(8);

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
    auto* productHint = new QLabel("Search or scan — press Enter to add the first match, or double-click a row.", productPanel);
    productHint->setObjectName("muted");

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
    productsTable_->verticalHeader()->setDefaultSectionSize(28);

    quantitySpin_ = new QSpinBox(productPanel);
    quantitySpin_->setRange(1, 1000000);
    quantitySpin_->setPrefix("Qty: ");
    quantitySpin_->setAccessibleName("Item quantity");

    addToCartBtn_ = new QPushButton("Add to cart", productPanel);
    addToCartBtn_->setObjectName("primary");

    auto* addRow = new QHBoxLayout;
    addRow->addWidget(quantitySpin_, 1);
    addRow->addWidget(addToCartBtn_, 2);

    productLayout->addWidget(productHeading);
    productLayout->addWidget(productHint);
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
    auto* cartHint = new QLabel("Select a line to adjust its quantity or remove it.", cartPanel);
    cartHint->setObjectName("muted");

    cartTable_ = new QTableWidget(cartPanel);
    cartTable_->setColumnCount(4);
    cartTable_->setHorizontalHeaderLabels({"Product", "Quantity", "Unit price", "Line total"});
    cartTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    cartTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    cartTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    cartTable_->setAlternatingRowColors(true);
    cartTable_->verticalHeader()->setDefaultSectionSize(28);

    minusBtn_ = new QPushButton("−1", cartPanel);
    plusBtn_ = new QPushButton("+1", cartPanel);
    removeLineBtn_ = new QPushButton("Remove line", cartPanel);
    removeLineBtn_->setObjectName("danger");

    auto* rowActions = new QHBoxLayout;
    rowActions->addWidget(minusBtn_);
    rowActions->addWidget(plusBtn_);
    rowActions->addWidget(removeLineBtn_);
    rowActions->addStretch();

    discountSpin_ = new QSpinBox(cartPanel);
    discountSpin_->setRange(0, 1000000000);
    discountSpin_->setPrefix("Invoice discount (paisa): ");
    discountSpin_->setAccessibleName("Invoice discount in paisa");
    
    auto* discountLabel = new QLabel("&Discount:", cartPanel);
    discountLabel->setBuddy(discountSpin_);

    auto* summary = new QFrame(cartPanel);
    summary->setObjectName("summaryBox");
    auto* sl = new QVBoxLayout(summary);
    sl->setContentsMargins(12, 8, 12, 8);
    sl->setSpacing(4);

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

    auto* discRow = new QHBoxLayout;
    discRow->addWidget(summaryDiscountLabel);
    discRow->addStretch();
    discRow->addWidget(discountValue_);

    totalValue_ = new QLabel("PKR 0.00", summary);
    totalValue_->setObjectName("posTotal");
    totalValue_->setAlignment(Qt::AlignRight);

    sl->addLayout(subRow);
    sl->addLayout(discRow);
    sl->addSpacing(2);
    sl->addWidget(totalValue_);

    receivedSpin_ = new QSpinBox(cartPanel);
    receivedSpin_->setRange(0, 2147483647);
    receivedSpin_->setPrefix("Amount received (paisa): ");
    receivedSpin_->setAccessibleName("Amount received in paisa");
    
    auto* receivedLabel = new QLabel("&Received:", cartPanel);
    receivedLabel->setBuddy(receivedSpin_);

    dueLabel_ = new QLabel("Change: PKR 0.00", cartPanel);
    dueLabel_->setObjectName("muted");

    savePrintBtn_ = new QPushButton("Save & &Print", cartPanel);
    savePrintBtn_->setObjectName("primary");
    savePrintBtn_->setMinimumHeight(32);

    saveBtn_ = new QPushButton("Sa&ve", cartPanel);
    holdBtn_ = new QPushButton("&Hold", cartPanel);
    resumeBtn_ = new QPushButton("R&esume", cartPanel);
    clearBtn_ = new QPushButton("C&lear", cartPanel);
    clearBtn_->setObjectName("danger");
    cancelBtn_ = new QPushButton("Cancel (&X)", cartPanel);

    auto* primaryRow = new QHBoxLayout;
    primaryRow->addWidget(savePrintBtn_, 3);
    primaryRow->addWidget(saveBtn_, 2);
    primaryRow->addWidget(holdBtn_, 2);

    auto* secondaryRow = new QHBoxLayout;
    secondaryRow->addWidget(resumeBtn_, 2);
    secondaryRow->addWidget(clearBtn_, 2);
    secondaryRow->addWidget(cancelBtn_, 2);

    feedbackLabel_ = new QLabel("", cartPanel);
    feedbackLabel_->setObjectName("muted");
    feedbackLabel_->setWordWrap(true);

    cartLayout->addWidget(cartHeading);
    cartLayout->addWidget(cartHint);
    cartLayout->addWidget(cartTable_, 1);
    cartLayout->addLayout(rowActions);
    cartLayout->addWidget(discountLabel);
    cartLayout->addWidget(discountSpin_);
    cartLayout->addWidget(summary);
    cartLayout->addWidget(receivedLabel);
    cartLayout->addWidget(receivedSpin_);
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
        const auto price = cartTable_->item(row, 2)->data(Qt::UserRole + 2).toLongLong();
        cartTable_->item(row, 3)->setText(pos::formatPaisa(newQuantity * price));
        cartTable_->item(row, 3)->setData(Qt::UserRole, newQuantity * price);
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
        const auto price = cartTable_->item(row, 2)->data(Qt::UserRole + 2).toLongLong();
        cartTable_->item(row, 1)->setText(QString::number(current - 1));
        cartTable_->item(row, 3)->setText(pos::formatPaisa((current - 1) * price));
        cartTable_->item(row, 3)->setData(Qt::UserRole, (current - 1) * price);
        refresh();
    });

    connect(removeLineBtn_, &QPushButton::clicked, this, [this] {
        const int row = cartTable_->currentRow();
        if (row >= 0) {
            cartTable_->removeRow(row);
            refresh();
        }
    });

    connect(discountSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
        const auto grand = computeTotal();
        if (paymentMethodCombo_->currentData().toString() != "credit") {
            receivedSpin_->setValue(grand);
        }
        refreshDue(grand);
    });

    connect(paymentMethodCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        const auto grand = computeTotal();
        if (paymentMethodCombo_->currentData().toString() != "credit") {
            receivedSpin_->setValue(grand);
        }
        refreshDue(grand);
    });

    connect(receivedSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
        refreshDue(computeTotal());
    });

    connect(saveBtn_, &QPushButton::clicked, this, [this] { completeSale(false); });
    connect(savePrintBtn_, &QPushButton::clicked, this, [this] { completeSale(true); });
    connect(holdBtn_, &QPushButton::clicked, this, &SalesPosPage::holdSale);
    connect(resumeBtn_, &QPushButton::clicked, this, &SalesPosPage::resumeSale);
    connect(clearBtn_, &QPushButton::clicked, this, &SalesPosPage::clearCart);
    connect(cancelBtn_, &QPushButton::clicked, this, [this] {
        if (cartTable_->rowCount() && QMessageBox::question(this, "Cancel sale", "Abandon this sale and return to the dashboard?") != QMessageBox::Yes) return;
        cartTable_->setRowCount(0);
        discountSpin_->setValue(0);
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
    setTabOrder(addToCartBtn_, cartTable_);
    setTabOrder(cartTable_, discountSpin_);
    setTabOrder(discountSpin_, receivedSpin_);
    setTabOrder(receivedSpin_, savePrintBtn_);
    setTabOrder(savePrintBtn_, saveBtn_);
    setTabOrder(saveBtn_, holdBtn_);
    setTabOrder(holdBtn_, resumeBtn_);
    setTabOrder(resumeBtn_, clearBtn_);
    setTabOrder(clearBtn_, cancelBtn_);
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
            
            auto* priceItem = new QTableWidgetItem("PKR " + pos::formatPaisa(item.retailPrice));
            priceItem->setData(Qt::UserRole + 2, item.retailPrice);
            productsTable_->setItem(row, 3, priceItem);
            
            productsTable_->setItem(row, 4, new QTableWidgetItem(item.baseUnit));
        }
    } catch (...) {}
}

qint64 SalesPosPage::computeTotal() {
    qint64 subtotal{};
    for (int row = 0; row < cartTable_->rowCount(); ++row) {
        subtotal += cartTable_->item(row, 3)->data(Qt::UserRole).toLongLong();
    }
    const qint64 disc = discountSpin_->value();
    const qint64 grand = subtotal - disc;
    subtotalValue_->setText("PKR " + pos::formatPaisa(subtotal));
    discountValue_->setText("PKR " + pos::formatPaisa(disc));
    totalValue_->setText("PKR " + pos::formatPaisa(grand));
    return grand;
}

void SalesPosPage::refreshDue(qint64 grand) {
    if (paymentMethodCombo_->currentData().toString() == "credit") {
        receivedSpin_->setEnabled(false);
        dueLabel_->setText(QString("Balance due: PKR %1").arg(pos::formatPaisa(grand)));
    } else {
        receivedSpin_->setEnabled(true);
        const auto paid = receivedSpin_->value();
        if (paid >= grand) {
            dueLabel_->setText(QString("Change: PKR %1").arg(pos::formatPaisa(paid - grand)));
        } else {
            dueLabel_->setText(QString("Balance due: PKR %1").arg(pos::formatPaisa(grand - paid)));
        }
    }
}

void SalesPosPage::refresh() {
    const auto grand = computeTotal();
    if (paymentMethodCombo_->currentData().toString() != "credit") {
        receivedSpin_->setValue(grand);
    }
    refreshDue(grand);
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
            cartTable_->item(current, 3)->setText(pos::formatPaisa(newQuantity * price));
            cartTable_->item(current, 3)->setData(Qt::UserRole, newQuantity * price);
            feedbackLabel_->clear();
            refresh();
            return;
        }
    }

    if (count > stock) {
        feedbackLabel_->setText(QString("Not enough stock — available: %1").arg(stock));
        return;
    }

    const int target = cartTable_->rowCount();
    cartTable_->insertRow(target);
    auto* name = new QTableWidgetItem(productsTable_->item(row, 0)->text());
    name->setData(Qt::UserRole, id);
    name->setData(Qt::UserRole + 1, productsTable_->item(row, 4)->text()); // base unit
    cartTable_->setItem(target, 0, name);
    cartTable_->setItem(target, 1, new QTableWidgetItem(QString::number(count)));
    
    auto* priceItem = new QTableWidgetItem(pos::formatPaisa(price));
    priceItem->setData(Qt::UserRole + 2, price);
    cartTable_->setItem(target, 2, priceItem);

    auto* amountItem = new QTableWidgetItem(pos::formatPaisa(count * price));
    amountItem->setData(Qt::UserRole, count * price);
    cartTable_->setItem(target, 3, amountItem);
    
    feedbackLabel_->clear();
    refresh();
}

void SalesPosPage::completeSale(bool printReceipt) {
    if (!cartTable_->rowCount()) {
        feedbackLabel_->setText("Add at least one product to the cart.");
        return;
    }
    try {
        pos::SaleRequest request;
        request.paymentMethod = paymentMethodCombo_->currentData().toString();
        request.customerId = customerCombo_->currentData().toString();
        request.invoiceDiscount = discountSpin_->value();

        qint64 subtotal{};
        for (int row = 0; row < cartTable_->rowCount(); ++row) {
            const auto count = cartTable_->item(row, 1)->text().toLongLong();
            const auto price = cartTable_->item(row, 2)->data(Qt::UserRole + 2).toLongLong();
            subtotal += count * price;
            request.lines.append({
                cartTable_->item(row, 0)->data(Qt::UserRole).toString(),
                {},
                count,
                price,
                0,
                cartTable_->item(row, 0)->data(Qt::UserRole + 1).toString()
            });
        }

        if (request.invoiceDiscount > subtotal) {
            QMessageBox::warning(this, "Invalid discount", "Discount cannot exceed the cart subtotal.");
            return;
        }

        request.paidAmount = request.paymentMethod == "credit" ? 0 : receivedSpin_->value();
        if (request.paidAmount > subtotal - request.invoiceDiscount) {
            request.paidAmount = subtotal - request.invoiceDiscount;
        }

        if (request.paymentMethod == "mixed") {
            const auto maxTender = request.paidAmount;
            bool ok = false;
            const auto cashAmt = QInputDialog::getInt(this, "Mixed payment", "Cash amount (paisa):", 0, 0, maxTender, 1, &ok);
            if (!ok) return;
            const auto chequeAmt = QInputDialog::getInt(this, "Mixed payment", "Cheque amount (paisa):", 0, 0, maxTender - cashAmt, 1, &ok);
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

        QList<pos::ThermalReceiptItem> receiptItems;
        for (int row = 0; row < cartTable_->rowCount(); ++row) {
            receiptItems.append({cartTable_->item(row, 0)->text(), cartTable_->item(row, 1)->text().toLongLong(), cartTable_->item(row, 3)->data(Qt::UserRole).toLongLong()});
        }

        const auto sale = posService_->completeSale(request);
        cartTable_->setRowCount(0);
        discountSpin_->setValue(0);
        receivedSpin_->setValue(0);
        paymentMethodCombo_->setCurrentIndex(0);
        customerCombo_->setCurrentIndex(0);
        refresh();
        load();

        if (printReceipt) {
            try {
                const auto path = pos::SettingsService(database_).value("printer.thermal_path");
                if (path.trimmed().isEmpty()) {
                    feedbackLabel_->setText(QString("Invoice %1 saved. No printer is configured, so no receipt was printed.").arg(sale.invoiceNo));
                } else {
                    pos::ThermalPrintService::writeRaw(path, pos::ThermalPrintService::receiptBytes(pos::SettingsService(database_).value("business.name", "Nexora POS"), sale.invoiceNo, receiptItems, sale.total));
                    feedbackLabel_->setText(QString("Invoice %1 saved and receipt printed.").arg(sale.invoiceNo));
                }
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
        discountSpin_->setValue(0);
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
        
        pos::InventoryService inv(database_);
        for (const auto& line : sale.lines) {
            auto product = database_->prepare("SELECT name, stock_quantity FROM products WHERE id=? AND is_deleted=0");
            product.bind(1, line.productId);
            if (!product.stepRow() || product.integer(1) < line.quantity) {
                throw pos::DatabaseError("a held product is unavailable or out of stock");
            }
            const int row = cartTable_->rowCount();
            cartTable_->insertRow(row);
            
            auto* name = new QTableWidgetItem(product.text(0));
            name->setData(Qt::UserRole, line.productId);
            name->setData(Qt::UserRole + 1, line.unit);
            cartTable_->setItem(row, 0, name);
            cartTable_->setItem(row, 1, new QTableWidgetItem(QString::number(line.quantity)));
            
            auto* price = new QTableWidgetItem(pos::formatPaisa(line.unitPrice));
            price->setData(Qt::UserRole + 2, line.unitPrice);
            cartTable_->setItem(row, 2, price);
            
            auto* amount = new QTableWidgetItem(pos::formatPaisa(line.quantity * line.unitPrice));
            amount->setData(Qt::UserRole, line.quantity * line.unitPrice);
            cartTable_->setItem(row, 3, amount);
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
        discountSpin_->setValue(0);
        refresh();
        feedbackLabel_->setText("Cart cleared.");
    }
}
