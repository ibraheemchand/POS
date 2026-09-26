#include "ui/pages/purchases_page.h"
#include "ui/pages/page_helper.h"
#include "ui/theme.h"
#include "core/database.h"
#include "core/purchase_service.h"
#include "core/inventory_service.h"
#include "core/supplier_service.h"
#include "core/bundle_service.h"
#include "core/data_change_bus.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QMessageBox>
#include <QInputDialog>
#include <QHeaderView>
#include <QTimer>

// Column layout of the purchase cart.
namespace { enum Col { ColProduct = 0, ColQty = 1, ColCost = 2, ColBatch = 3, ColExpiry = 4 }; }

PurchasesPage::PurchasesPage(std::shared_ptr<pos::Database> database, QWidget* parent)
    : QWidget(parent), database_(database) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* title = new QLabel("Purchases", this);
    title->setObjectName("pageTitle");
    refreshListsBtn_ = new QPushButton("Refresh lists", this);
    auto* topRow = new QHBoxLayout;
    topRow->addWidget(title);
    topRow->addStretch();
    topRow->addWidget(refreshListsBtn_);
    layout->addLayout(topRow);

    supplierCombo_ = new QComboBox(this);
    supplierCombo_->setObjectName("purchaseSupplier");
    supplierCombo_->setPlaceholderText("Select supplier");

    auto* supplierLabel = new QLabel("Supplier (&S):", this);
    supplierLabel->setBuddy(supplierCombo_);

    auto* supRow = new QHBoxLayout;
    supRow->addWidget(supplierLabel);
    supRow->addWidget(supplierCombo_, 1);
    layout->addLayout(supRow);

    auto* controls1 = new QHBoxLayout;
    productCombo_ = new QComboBox(this);
    productCombo_->setObjectName("purchaseProduct");
    productCombo_->setPlaceholderText("Select product");

    auto* productLabel = new QLabel("&Product:", this);
    productLabel->setBuddy(productCombo_);

    quantitySpin_ = new QSpinBox(this);
    quantitySpin_->setRange(1, 1000000);

    auto* quantityLabel = new QLabel("&Qty:", this);
    quantityLabel->setBuddy(quantitySpin_);

    priceSpin_ = new QDoubleSpinBox(this);
    priceSpin_->setRange(0, 100000000);
    priceSpin_->setDecimals(2);
    priceSpin_->setSuffix(" PKR");

    auto* costLabel = new QLabel("Unit &Cost (PKR):", this);
    costLabel->setBuddy(priceSpin_);

    controls1->addWidget(productLabel);
    controls1->addWidget(productCombo_, 3);
    controls1->addWidget(quantityLabel);
    controls1->addWidget(quantitySpin_, 1);
    controls1->addWidget(costLabel);
    controls1->addWidget(priceSpin_, 2);
    layout->addLayout(controls1);

    auto* controls2 = new QHBoxLayout;
    batchInput_ = new QLineEdit(this);
    batchInput_->setPlaceholderText("Batch number (optional)");

    auto* batchLabel = new QLabel("&Batch:", this);
    batchLabel->setBuddy(batchInput_);

    expiryEdit_ = new QDateEdit(QDate::currentDate().addYears(1), this);
    expiryEdit_->setCalendarPopup(true);

    auto* expiryLabel = new QLabel("&Expiry:", this);
    expiryLabel->setBuddy(expiryEdit_);

    addBtn_ = new QPushButton("Add to cart", this);
    addBtn_->setObjectName("primary");
    loadCourseBtn_ = new QPushButton("Load course…", this);

    controls2->addWidget(batchLabel);
    controls2->addWidget(batchInput_, 2);
    controls2->addWidget(expiryLabel);
    controls2->addWidget(expiryEdit_, 2);
    controls2->addWidget(addBtn_, 1);
    controls2->addWidget(loadCourseBtn_, 1);
    layout->addLayout(controls2);

    // Course-loading row: enter how many sets, then load every book of a course at once.
    auto* controls3 = new QHBoxLayout;
    setsSpin_ = new QSpinBox(this);
    setsSpin_->setRange(1, 1000000);
    setsSpin_->setValue(1);
    auto* setsLabel = new QLabel("&Number of sets:", this);
    setsLabel->setBuddy(setsSpin_);
    controls3->addWidget(setsLabel);
    controls3->addWidget(setsSpin_, 1);
    controls3->addStretch();
    layout->addLayout(controls3);

    cartTable_ = new QTableWidget(this);
    cartTable_->setColumnCount(5);
    cartTable_->setHorizontalHeaderLabels({"Product", "Quantity", "Unit cost", "Batch", "Expiry"});
    cartTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    cartTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    cartTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    // Quantity, cost and batch are edited inline; other columns stay read-only (per cell below).
    cartTable_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    layout->addWidget(cartTable_, 1);

    auto* cartActions = new QHBoxLayout;
    removeBtn_ = new QPushButton("Remove item", this);
    removeBtn_->setObjectName("danger");
    clearBtn_ = new QPushButton("Clear cart", this);
    cartActions->addWidget(removeBtn_);
    cartActions->addWidget(clearBtn_);
    cartActions->addStretch();
    layout->addLayout(cartActions);

    // Totals box: item count, total quantity, purchase total. Styled as a "panel"
    // card (not "summaryBox", which the Sales POS layout test looks up by name).
    auto* summary = new QFrame(this);
    summary->setObjectName("panel");
    auto* summaryRow = new QHBoxLayout(summary);
    summaryRow->setContentsMargins(10, 6, 10, 6);
    itemsLabel_ = new QLabel("Items: 0", summary);
    totalQtyLabel_ = new QLabel("Total qty: 0", summary);
    totalLabel_ = new QLabel("Total: PKR 0", summary);
    totalLabel_->setObjectName("posTotal");
    summaryRow->addWidget(itemsLabel_);
    summaryRow->addStretch();
    summaryRow->addWidget(totalQtyLabel_);
    summaryRow->addStretch();
    summaryRow->addWidget(totalLabel_);
    layout->addWidget(summary);

    // Payment method: Cash (default) / Credit / Partial.
    auto* payRow = new QHBoxLayout;
    auto* payLabel = new QLabel("Pay&ment:", this);
    paymentCombo_ = new QComboBox(this);
    paymentCombo_->addItem("Cash", "cash");
    paymentCombo_->addItem("Credit", "credit");
    paymentCombo_->addItem("Partial", "partial");
    payLabel->setBuddy(paymentCombo_);
    payRow->addWidget(payLabel);
    payRow->addWidget(paymentCombo_, 1);
    payRow->addStretch(2);
    layout->addLayout(payRow);

    // Partial-payment fields (hidden unless Partial is selected).
    auto* partialRow = new QHBoxLayout;
    paidLabel_ = new QLabel("Amount paid now (PKR):", this);
    paidSpin_ = new QDoubleSpinBox(this);
    paidSpin_->setRange(0, 100000000);
    paidSpin_->setDecimals(2);
    remainingLabel_ = new QLabel("Remaining payable: PKR 0", this);
    partialRow->addWidget(paidLabel_);
    partialRow->addWidget(paidSpin_, 1);
    partialRow->addWidget(remainingLabel_, 1);
    partialRow->addStretch();
    layout->addLayout(partialRow);

    saveBtn_ = new QPushButton("Receive purchase (Cash)", this);
    saveBtn_->setObjectName("primary");
    layout->addWidget(saveBtn_);

    // Initial setups
    reloadLists();
    QTimer::singleShot(0, this, [this] { reloadLists(); });

    // Connections
    connect(addBtn_, &QPushButton::clicked, this, [this] {
        if (productCombo_->currentIndex() < 0) {
            QMessageBox::information(this, "No product selected", "Please select a product from the list. If you haven't created any products yet, go to the 'Inventory' tab to add them first.");
            return;
        }
        const auto id = productCombo_->currentData().toString();
        pos::InventoryService service(database_);
        const auto trackInfo = service.getProductTrackingInfo(id);
        const auto row = addRow(id, productCombo_->currentText(), trackInfo.first, trackInfo.second,
                                quantitySpin_->value(), pos::roundMoney(priceSpin_->value() * 100),
                                batchInput_->text().trimmed(), expiryEdit_->date());
        cartTable_->setCurrentCell(row, ColQty);
        batchInput_->clear();
    });

    connect(loadCourseBtn_, &QPushButton::clicked, this, &PurchasesPage::loadCourse);

    connect(cartTable_, &QTableWidget::cellChanged, this, &PurchasesPage::onCellChanged);

    connect(removeBtn_, &QPushButton::clicked, this, [this] {
        const int row = cartTable_->currentRow();
        if (row >= 0) { cartTable_->removeRow(row); recomputeTotal(); }
    });

    connect(clearBtn_, &QPushButton::clicked, this, [this] {
        cartTable_->setRowCount(0);
        recomputeTotal();
    });

    connect(refreshListsBtn_, &QPushButton::clicked, this, &PurchasesPage::reloadLists);

    connect(paymentCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { updatePaymentUi(); });
    connect(paidSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { updatePaymentUi(); });
    updatePaymentUi();

    connect(saveBtn_, &QPushButton::clicked, this, [this] {
        if (supplierCombo_->currentIndex() < 0 || cartTable_->rowCount() == 0) {
            QMessageBox::information(this, "Purchase required", "Choose a supplier and add at least one item.");
            return;
        }
        // Block: every batch-tracked row must have a batch number.
        QStringList missingBatch;
        for (int row = 0; row < cartTable_->rowCount(); ++row) {
            const auto* name = cartTable_->item(row, ColProduct);
            if (name->data(Qt::UserRole + 2).toBool() && cartTable_->item(row, ColBatch)->text().trimmed().isEmpty()) {
                missingBatch.append(QString("• %1 (row %2)").arg(name->text()).arg(row + 1));
            }
        }
        if (!missingBatch.isEmpty()) {
            QMessageBox::warning(this, "Batch number required",
                "These batch-tracked books need a batch number before you can save. Double-click the Batch cell to enter one:\n\n" + missingBatch.join("\n"));
            return;
        }
        // Warn: any book with 0 cost (e.g. never purchased before).
        QStringList zeroCost;
        for (int row = 0; row < cartTable_->rowCount(); ++row) {
            if (cartTable_->item(row, ColCost)->data(Qt::UserRole).toLongLong() == 0) {
                zeroCost.append("• " + cartTable_->item(row, ColProduct)->text());
            }
        }
        if (!zeroCost.isEmpty()) {
            const auto choice = QMessageBox::question(this, "Books with no cost",
                "These books have a unit cost of 0:\n\n" + zeroCost.join("\n") + "\n\nSave the purchase anyway?",
                QMessageBox::Save | QMessageBox::Cancel, QMessageBox::Cancel);
            if (choice != QMessageBox::Save) return;
        }
        // Resolve payment: Cash pays the full total, Credit pays nothing, Partial pays
        // a validated amount (more than 0, less than the total) with the rest on credit.
        const auto total = cartTotal();
        const auto method = paymentCombo_->currentData().toString();
        qint64 paid = 0;
        QString requestMethod = "credit";
        if (method == "cash") { requestMethod = "cash"; paid = total; }
        else if (method == "partial") {
            requestMethod = "cash";
            paid = pos::roundMoney(paidSpin_->value() * 100);
            if (paid <= 0 || paid >= total) {
                QMessageBox::warning(this, "Invalid partial payment",
                    QString("The amount paid now must be more than 0 and less than the total (PKR %1).").arg(pos::formatMoney(total)));
                return;
            }
        }
        try {
            pos::PurchaseRequest request;
            request.supplierId = supplierCombo_->currentData().toString();
            request.paymentMethod = requestMethod;
            request.paidAmount = paid;
            for (int row = 0; row < cartTable_->rowCount(); ++row) {
                const auto* nameItem = cartTable_->item(row, ColProduct);
                request.lines.append({
                    nameItem->data(Qt::UserRole).toString(),
                    cartTable_->item(row, ColQty)->data(Qt::UserRole).toLongLong(),
                    cartTable_->item(row, ColCost)->data(Qt::UserRole).toLongLong(),
                    0, 0,
                    nameItem->data(Qt::UserRole + 1).toString(),
                    cartTable_->item(row, ColBatch)->text().trimmed(),
                    QDate::fromString(cartTable_->item(row, ColExpiry)->text(), Qt::ISODate)
                });
            }
            const auto result = pos::PurchaseService(database_).completePurchase(request);
            cartTable_->setRowCount(0);
            paidSpin_->setValue(0);
            paymentCombo_->setCurrentIndex(0);
            recomputeTotal();
            QMessageBox::information(this, "Purchase received", QString("Purchase %1 saved.").arg(result.invoiceNo));
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Purchase failed", error.what());
        }
    });

    // Auto update wiring
    connect(&pos::DataChangeBus::instance(), &pos::DataChangeBus::inventoryChanged, this, &PurchasesPage::reloadLists);
    connect(&pos::DataChangeBus::instance(), &pos::DataChangeBus::suppliersChanged, this, &PurchasesPage::reloadLists);
    connect(&pos::DataChangeBus::instance(), &pos::DataChangeBus::purchasesChanged, this, &PurchasesPage::reloadLists);

    // Set tab order
    setTabOrder(supplierCombo_, productCombo_);
    setTabOrder(productCombo_, quantitySpin_);
    setTabOrder(quantitySpin_, priceSpin_);
    setTabOrder(priceSpin_, batchInput_);
    setTabOrder(batchInput_, expiryEdit_);
    setTabOrder(expiryEdit_, addBtn_);
    setTabOrder(addBtn_, loadCourseBtn_);
    setTabOrder(loadCourseBtn_, setsSpin_);
    setTabOrder(setsSpin_, cartTable_);
    setTabOrder(cartTable_, removeBtn_);
    setTabOrder(removeBtn_, clearBtn_);
    setTabOrder(clearBtn_, paymentCombo_);
    setTabOrder(paymentCombo_, paidSpin_);
    setTabOrder(paidSpin_, saveBtn_);
    setTabOrder(saveBtn_, refreshListsBtn_);
}

int PurchasesPage::addRow(const QString& productId, const QString& name, const QString& unit,
                          bool tracked, qint64 quantity, qint64 unitCost, const QString& batch, const QDate& expiry) {
    suppressCellChange_ = true;
    const int row = cartTable_->rowCount();
    cartTable_->insertRow(row);

    auto* nameItem = new QTableWidgetItem(name);
    nameItem->setData(Qt::UserRole, productId);
    nameItem->setData(Qt::UserRole + 1, unit);
    nameItem->setData(Qt::UserRole + 2, tracked);
    nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
    cartTable_->setItem(row, ColProduct, nameItem);

    auto* qtyItem = new QTableWidgetItem(QString::number(quantity));
    qtyItem->setData(Qt::UserRole, quantity);
    cartTable_->setItem(row, ColQty, qtyItem);

    auto* costItem = new QTableWidgetItem("PKR " + pos::formatMoney(unitCost));
    costItem->setData(Qt::UserRole, unitCost);
    cartTable_->setItem(row, ColCost, costItem);

    cartTable_->setItem(row, ColBatch, new QTableWidgetItem(batch));

    auto* expiryItem = new QTableWidgetItem(expiry.toString(Qt::ISODate));
    expiryItem->setFlags(expiryItem->flags() & ~Qt::ItemIsEditable);
    cartTable_->setItem(row, ColExpiry, expiryItem);

    suppressCellChange_ = false;
    refreshRowState(row);
    recomputeTotal();
    return row;
}

void PurchasesPage::loadCourse() {
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

        const auto sets = setsSpin_->value();
        const auto items = pos::BundleService(database_).resolveItems(bundle.id);
        pos::InventoryService inv(database_);
        int added = 0;
        for (const auto& item : items) {
            const qint64 quantity = sets * item.quantity;
            const qint64 cost = item.purchasePrice; // book's last purchase cost (0 if never purchased)
            // Merge into an existing row only when product, unit AND cost match.
            bool merged = false;
            for (int row = 0; row < cartTable_->rowCount(); ++row) {
                if (cartTable_->item(row, ColProduct)->data(Qt::UserRole).toString() == item.productId
                    && cartTable_->item(row, ColProduct)->data(Qt::UserRole + 1).toString() == item.baseUnit
                    && cartTable_->item(row, ColCost)->data(Qt::UserRole).toLongLong() == cost) {
                    const auto newQty = cartTable_->item(row, ColQty)->data(Qt::UserRole).toLongLong() + quantity;
                    suppressCellChange_ = true;
                    cartTable_->item(row, ColQty)->setData(Qt::UserRole, newQty);
                    cartTable_->item(row, ColQty)->setText(QString::number(newQty));
                    suppressCellChange_ = false;
                    merged = true;
                    break;
                }
            }
            if (!merged) {
                const auto trackInfo = inv.getProductTrackingInfo(item.productId);
                addRow(item.productId, item.productName, item.baseUnit, trackInfo.second,
                       quantity, cost, {}, QDate::currentDate().addYears(1));
            }
            ++added;
        }
        recomputeTotal();
        QMessageBox::information(this, "Course loaded",
            QString("Loaded %1 book(s) from %2 (%3 set(s)). Review quantities, costs and batch numbers before saving.")
                .arg(added).arg(bundle.name).arg(sets));
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Could not load course", error.what());
    }
}

qint64 PurchasesPage::cartTotal() const {
    qint64 total = 0;
    for (int row = 0; row < cartTable_->rowCount(); ++row) {
        total += cartTable_->item(row, ColQty)->data(Qt::UserRole).toLongLong()
               * cartTable_->item(row, ColCost)->data(Qt::UserRole).toLongLong();
    }
    return total;
}

void PurchasesPage::recomputeTotal() {
    qint64 totalQty = 0;
    for (int row = 0; row < cartTable_->rowCount(); ++row) {
        totalQty += cartTable_->item(row, ColQty)->data(Qt::UserRole).toLongLong();
    }
    itemsLabel_->setText(QString("Items: %1").arg(cartTable_->rowCount()));
    totalQtyLabel_->setText(QString("Total qty: %1").arg(totalQty));
    totalLabel_->setText("Total: PKR " + pos::formatMoney(cartTotal()));
    updatePaymentUi();
}

void PurchasesPage::updatePaymentUi() {
    const auto method = paymentCombo_->currentData().toString();
    const bool partial = method == "partial";
    paidLabel_->setVisible(partial);
    paidSpin_->setVisible(partial);
    remainingLabel_->setVisible(partial);

    const auto total = cartTotal();
    if (partial) {
        const qint64 paid = pos::roundMoney(paidSpin_->value() * 100);
        const qint64 remaining = total - paid;
        remainingLabel_->setText("Remaining payable: PKR " + pos::formatMoney(remaining > 0 ? remaining : 0));
        saveBtn_->setText("Receive purchase (Partial)");
    } else if (method == "credit") {
        saveBtn_->setText("Receive purchase on credit");
    } else {
        saveBtn_->setText("Receive purchase (Cash)");
    }
}

void PurchasesPage::refreshRowState(int row) {
    if (row < 0 || row >= cartTable_->rowCount()) return;
    // Colour changes emit cellChanged too; suppress so this never re-enters onCellChanged.
    const bool prev = suppressCellChange_;
    suppressCellChange_ = true;
    auto* costItem = cartTable_->item(row, ColCost);
    auto* batchItem = cartTable_->item(row, ColBatch);
    const bool tracked = cartTable_->item(row, ColProduct)->data(Qt::UserRole + 2).toBool();
    const bool dark = pos::theme::isDark();

    const QColor amberBg = dark ? QColor("#3a2e05") : QColor("#ffe9b3");
    const QColor amberFg = dark ? QColor("#ffcf5a") : QColor("#7a5300");
    const QColor redBg   = dark ? QColor("#4a1512") : QColor("#ffdad6");
    const QColor redFg   = dark ? QColor("#ffb4ab") : QColor("#ba1a1a");
    const QColor clearBg = dark ? QColor("#1E2025") : QColor("#ffffff");
    const QColor clearFg = dark ? QColor("#E2E2E9") : QColor("#1a1c1c");

    if (costItem->data(Qt::UserRole).toLongLong() == 0) {
        costItem->setBackground(amberBg); costItem->setForeground(amberFg);
    } else {
        costItem->setBackground(clearBg); costItem->setForeground(clearFg);
    }
    if (tracked && batchItem->text().trimmed().isEmpty()) {
        batchItem->setBackground(redBg); batchItem->setForeground(redFg);
    } else {
        batchItem->setBackground(clearBg); batchItem->setForeground(clearFg);
    }
    suppressCellChange_ = prev;
}

void PurchasesPage::onCellChanged(int row, int column) {
    if (suppressCellChange_) return;
    auto* item = cartTable_->item(row, column);
    if (!item) return;
    suppressCellChange_ = true;

    if (column == ColQty) {
        bool ok = false;
        const qint64 qty = item->text().trimmed().toLongLong(&ok);
        if (!ok || qty <= 0) {
            QMessageBox::warning(this, "Invalid quantity", "Quantity must be a whole number greater than 0.");
            item->setText(QString::number(item->data(Qt::UserRole).toLongLong())); // revert
        } else {
            item->setData(Qt::UserRole, qty);
        }
    } else if (column == ColCost) {
        bool ok = false;
        const pos::Money cost = pos::parseMoney(item->text(), &ok);
        if (!ok || cost < 0) {
            QMessageBox::warning(this, "Invalid unit cost", "Enter a valid cost in rupees — 0 or more, at most 2 decimals.");
            item->setText("PKR " + pos::formatMoney(item->data(Qt::UserRole).toLongLong())); // revert
        } else {
            item->setData(Qt::UserRole, static_cast<qint64>(cost));
            item->setText("PKR " + pos::formatMoney(cost)); // normalise display
        }
    }
    // ColBatch: free text, no parsing — just refresh the missing-batch highlight below.

    suppressCellChange_ = false;
    refreshRowState(row);
    recomputeTotal();
}

void PurchasesPage::reloadLists() {
    supplierCombo_->clear();
    productCombo_->clear();
    try {
        const auto activeSuppliers = pos::SupplierService(database_).listActive();
        for (const auto& item : activeSuppliers) {
            supplierCombo_->addItem(item.name, item.id);
        }

        const auto activeProducts = pos::InventoryService(database_).listActiveSummaries();
        for (const auto& item : activeProducts) {
            productCombo_->addItem(item.name + " (" + item.baseUnit + ")", item.id);
        }
    } catch (...) {}
}
