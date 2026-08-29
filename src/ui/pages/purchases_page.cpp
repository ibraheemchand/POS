#include "ui/pages/purchases_page.h"
#include "ui/pages/page_helper.h"
#include "core/database.h"
#include "core/purchase_service.h"
#include "core/inventory_service.h"
#include "core/supplier_service.h"
#include "core/data_change_bus.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QHeaderView>
#include <QTimer>

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

    priceSpin_ = new QSpinBox(this);
    priceSpin_->setRange(0, 1000000000);
    priceSpin_->setSuffix(" paisa");
    
    auto* costLabel = new QLabel("Unit &Cost (paisa):", this);
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

    controls2->addWidget(batchLabel);
    controls2->addWidget(batchInput_, 2);
    controls2->addWidget(expiryLabel);
    controls2->addWidget(expiryEdit_, 2);
    controls2->addWidget(addBtn_, 1);
    layout->addLayout(controls2);

    cartTable_ = new QTableWidget(this);
    cartTable_->setColumnCount(5);
    cartTable_->setHorizontalHeaderLabels({"Product", "Quantity", "Unit cost", "Batch", "Expiry"});
    cartTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    cartTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    cartTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    cartTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(cartTable_, 1);

    auto* cartActions = new QHBoxLayout;
    removeBtn_ = new QPushButton("Remove item", this);
    removeBtn_->setObjectName("danger");
    clearBtn_ = new QPushButton("Clear cart", this);
    
    cartActions->addWidget(removeBtn_);
    cartActions->addWidget(clearBtn_);
    cartActions->addStretch();
    layout->addLayout(cartActions);

    saveBtn_ = new QPushButton("Receive purchase on credit", this);
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
        const bool tracked = trackInfo.second;
        if (tracked && batchInput_->text().trimmed().isEmpty()) {
            QMessageBox::information(this, "Batch required", "This product tracks batches. Enter a batch number.");
            return;
        }

        const int row = cartTable_->rowCount();
        cartTable_->insertRow(row);
        auto* name = new QTableWidgetItem(productCombo_->currentText());
        name->setData(Qt::UserRole, id);
        name->setData(Qt::UserRole + 1, trackInfo.first); // base unit
        cartTable_->setItem(row, 0, name);
        cartTable_->setItem(row, 1, new QTableWidgetItem(QString::number(quantitySpin_->value())));
        auto* costItem = new QTableWidgetItem("PKR " + pos::formatPaisa(priceSpin_->value()));
        costItem->setData(Qt::UserRole, static_cast<qint64>(priceSpin_->value()));
        cartTable_->setItem(row, 2, costItem);
        cartTable_->setItem(row, 3, new QTableWidgetItem(batchInput_->text().trimmed()));
        cartTable_->setItem(row, 4, new QTableWidgetItem(expiryEdit_->date().toString(Qt::ISODate)));
        batchInput_->clear();
    });

    connect(removeBtn_, &QPushButton::clicked, this, [this] {
        const int row = cartTable_->currentRow();
        if (row >= 0) cartTable_->removeRow(row);
    });

    connect(clearBtn_, &QPushButton::clicked, this, [this] {
        cartTable_->setRowCount(0);
    });

    connect(refreshListsBtn_, &QPushButton::clicked, this, &PurchasesPage::reloadLists);

    connect(saveBtn_, &QPushButton::clicked, this, [this] {
        if (supplierCombo_->currentIndex() < 0 || cartTable_->rowCount() == 0) {
            QMessageBox::information(this, "Purchase required", "Choose a supplier and add at least one item.");
            return;
        }
        try {
            pos::PurchaseRequest request;
            request.supplierId = supplierCombo_->currentData().toString();
            for (int row = 0; row < cartTable_->rowCount(); ++row) {
                const auto* nameItem = cartTable_->item(row, 0);
                request.lines.append({
                    nameItem->data(Qt::UserRole).toString(),
                    cartTable_->item(row, 1)->text().toLongLong(),
                    cartTable_->item(row, 2)->data(Qt::UserRole).toLongLong(),
                    0, 0,
                    nameItem->data(Qt::UserRole + 1).toString(),
                    cartTable_->item(row, 3)->text(),
                    QDate::fromString(cartTable_->item(row, 4)->text(), Qt::ISODate)
                });
            }
            const auto result = pos::PurchaseService(database_).completePurchase(request);
            cartTable_->setRowCount(0);
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
    setTabOrder(addBtn_, cartTable_);
    setTabOrder(cartTable_, removeBtn_);
    setTabOrder(removeBtn_, clearBtn_);
    setTabOrder(clearBtn_, saveBtn_);
    setTabOrder(saveBtn_, refreshListsBtn_);
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
