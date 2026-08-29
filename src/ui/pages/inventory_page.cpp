#include "ui/pages/inventory_page.h"
#include "ui/pages/page_helper.h"
#include "core/database.h"
#include "core/inventory_service.h"
#include "core/settings_service.h"
#include "core/thermal_print_service.h"
#include "core/data_change_bus.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QShortcut>
#include <QKeySequence>
#include <QApplication>

InventoryPage::InventoryPage(std::shared_ptr<pos::Database> database, QWidget* parent)
    : QWidget(parent), database_(database) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    
    // Page Header & Caption
    auto* headerLayout = new QHBoxLayout;
    auto* titleContainer = new QVBoxLayout;
    auto* title = new QLabel("Inventory", this);
    title->setObjectName("pageTitle");
    titleContainer->addWidget(title);
    
    auto* subtitle = new QLabel("Manage stock, track SKUs, and monitor valuation across Main Branch.", this);
    subtitle->setObjectName("muted");
    titleContainer->addWidget(subtitle);
    headerLayout->addLayout(titleContainer);
    headerLayout->addStretch();
    layout->addLayout(headerLayout);

    // Controls Row
    auto* controls = new QHBoxLayout;
    auto* searchLabel = new QLabel("Find Product (&F):", this);
    search_ = new QLineEdit(this);
    search_->setObjectName("inventorySearch");
    search_->setPlaceholderText("Search product, SKU or barcode...");
    search_->setMinimumWidth(280);
    search_->setMaximumWidth(400);
    searchLabel->setBuddy(search_);

    auto* filterBtn = new QPushButton("Filters", this);
    addBtn_ = new QPushButton("&Add New Item", this);
    addBtn_->setObjectName("primary");

    controls->addWidget(searchLabel);
    controls->addWidget(search_);
    controls->addWidget(filterBtn);
    controls->addWidget(addBtn_);
    controls->addStretch();
    layout->addLayout(controls);

    // Quick Stats Grid Layout
    auto* statsGrid = new QGridLayout;
    statsGrid->setSpacing(20);
    
    auto* card1 = new QFrame(this); card1->setObjectName("metric");
    auto* l1 = new QVBoxLayout(card1);
    auto* lbl1 = new QLabel("TOTAL SKUS", card1); lbl1->setObjectName("metricLabel");
    val1_ = new QLabel("0", card1); val1_->setObjectName("metricValue");
    auto* cap1 = new QLabel("+0 this week", card1); cap1->setObjectName("metricCaption");
    l1->addWidget(lbl1); l1->addWidget(val1_); l1->addWidget(cap1);
    
    auto* card2 = new QFrame(this); card2->setObjectName("metric");
    auto* l2 = new QVBoxLayout(card2);
    auto* lbl2 = new QLabel("LOW STOCK ITEMS", card2); lbl2->setObjectName("metricLabel");
    val2_ = new QLabel("0", card2); val2_->setObjectName("metricValue"); val2_->setStyleSheet("color: #E9C349;");
    auto* cap2 = new QLabel("Requires reorder", card2); cap2->setObjectName("metricCaption");
    l2->addWidget(lbl2); l2->addWidget(val2_); l2->addWidget(cap2);
    
    auto* card3 = new QFrame(this); card3->setObjectName("metric");
    auto* l3 = new QVBoxLayout(card3);
    auto* lbl3 = new QLabel("OUT OF STOCK", card3); lbl3->setObjectName("metricLabel");
    val3_ = new QLabel("0", card3); val3_->setObjectName("metricValue"); val3_->setStyleSheet("color: #FFB4AB;");
    auto* cap3 = new QLabel("Critical attention", card3); cap3->setObjectName("metricCaption");
    l3->addWidget(lbl3); l3->addWidget(val3_); l3->addWidget(cap3);
    
    auto* card4 = new QFrame(this); card4->setObjectName("metric");
    auto* l4 = new QVBoxLayout(card4);
    auto* lbl4 = new QLabel("TOTAL INVENTORY VALUE", card4); lbl4->setObjectName("metricLabel");
    val4_ = new QLabel("PKR 0.00", card4); val4_->setObjectName("metricValue");
    auto* cap4 = new QLabel("Based on retail price", card4); cap4->setObjectName("metricCaption");
    l4->addWidget(lbl4); l4->addWidget(val4_); l4->addWidget(cap4);
    
    statsGrid->addWidget(card1, 0, 0); statsGrid->addWidget(card2, 0, 1);
    statsGrid->addWidget(card3, 0, 2); statsGrid->addWidget(card4, 0, 3);
    layout->addLayout(statsGrid);

    // High Density Table Panel
    auto* tablePanel = new QFrame(this); tablePanel->setObjectName("panel");
    auto* tableLayout = new QVBoxLayout(tablePanel); tableLayout->setContentsMargins(16, 16, 16, 16); tableLayout->setSpacing(12);
    
    auto* tableToolbar = new QHBoxLayout;
    resultsCount_ = new QLabel("Showing 0 results", tablePanel); resultsCount_->setObjectName("muted");
    editBtn_ = new QPushButton("&Edit selected", tablePanel);
    archiveBtn_ = new QPushButton("Ar&chive selected", tablePanel); archiveBtn_->setObjectName("danger");
    printLabelBtn_ = new QPushButton("&Print barcode label", tablePanel);
    importCsvBtn_ = new QPushButton("&Import CSV", tablePanel);
    refreshBtn_ = new QPushButton("&Refresh", tablePanel);

    tableToolbar->addWidget(resultsCount_); tableToolbar->addStretch();
    tableToolbar->addWidget(editBtn_); tableToolbar->addWidget(archiveBtn_);
    tableToolbar->addWidget(printLabelBtn_); tableToolbar->addWidget(importCsvBtn_); tableToolbar->addWidget(refreshBtn_);
    tableLayout->addLayout(tableToolbar);

    table_ = new QTableWidget(tablePanel);
    table_->setObjectName("inventoryTable");
    table_->setColumnCount(7);
    table_->setHorizontalHeaderLabels({"SKU / CODE", "ITEM NAME", "CATEGORY", "STOCK LEVEL", "UNIT PRICE", "TOTAL VALUE", "ACTIONS"});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    table_->setColumnWidth(3, 148);
    table_->setColumnWidth(6, 88);
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(44);
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    tableLayout->addWidget(table_, 1);

    // Pagination Footer
    auto* paginationRow = new QHBoxLayout;
    paginationText_ = new QLabel("Showing 1-250 of 250 items", tablePanel); paginationText_->setObjectName("muted");
    auto* prevBtn = new QPushButton("◀", tablePanel); prevBtn->setFixedWidth(36);
    pageNum_ = new QLabel("Page 1 of 1", tablePanel); pageNum_->setObjectName("muted");
    auto* nextBtn = new QPushButton("▶", tablePanel); nextBtn->setFixedWidth(36);
    paginationRow->addWidget(paginationText_); paginationRow->addStretch();
    paginationRow->addWidget(prevBtn); paginationRow->addWidget(pageNum_); paginationRow->addWidget(nextBtn);
    tableLayout->addLayout(paginationRow);
    layout->addWidget(tablePanel, 1);

    // Connect Events
    QTimer::singleShot(0, this, [this] { load(); });
    connect(search_, &QLineEdit::textChanged, this, [this] { load(); });
    connect(table_, &QTableWidget::activated, this, [this](const QModelIndex&) { editBtn_->click(); });
    
    auto* delShortcut = new QShortcut(QKeySequence::Delete, table_); 
    connect(delShortcut, &QShortcut::activated, this, [this] { archiveBtn_->click(); });
    connect(refreshBtn_, &QPushButton::clicked, this, [this] { load(); });
    connect(&pos::DataChangeBus::instance(), &pos::DataChangeBus::inventoryChanged, this, &InventoryPage::load);

    connect(addBtn_, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const auto name = QInputDialog::getText(this, "New product", "Product name:", QLineEdit::Normal, {}, &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        const auto unit = QInputDialog::getText(this, "New product", "Base unit:", QLineEdit::Normal, "piece", &ok);
        if (!ok) return;
        const auto retail = QInputDialog::getInt(this, "New product", "Retail price (paisa):", 0, 0, 1000000000, 1, &ok);
        if (!ok) return;
        try {
            pos::InventoryService inventory(database_);
            inventory.createProduct(name, unit, 0, retail, false);
            load();
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not add product", error.what());
        }
    });

    connect(editBtn_, &QPushButton::clicked, this, [this] {
        const int row = table_->currentRow();
        if (row < 0) {
            QMessageBox::information(this, "Edit product", "Select a product first.");
            return;
        }
        const auto id = table_->item(row, 0)->data(Qt::UserRole).toString();
        try {
            pos::InventoryService service(database_);
            auto query = service.findProduct(id);
            bool ok = false;
            const auto name = QInputDialog::getText(this, "Edit product", "Product name:", QLineEdit::Normal, query.name, &ok);
            if (!ok) return;
            const auto unit = QInputDialog::getText(this, "Edit product", "Base unit:", QLineEdit::Normal, query.baseUnit, &ok);
            if (!ok) return;
            const auto purchase = QInputDialog::getInt(this, "Edit product", "Purchase price (paisa):", query.purchasePrice, 0, 1000000000, 1, &ok);
            if (!ok) return;
            const auto retail = QInputDialog::getInt(this, "Edit product", "Retail price (paisa):", query.retailPrice, 0, 1000000000, 1, &ok);
            if (!ok) return;
            const auto minimum = QInputDialog::getInt(this, "Edit product", "Minimum stock:", query.minimumStock, 0, 1000000000, 1, &ok);
            if (!ok) return;

            pos::ProductDefinition product = query;
            product.name = name;
            product.baseUnit = unit;
            product.purchasePrice = purchase;
            product.retailPrice = retail;
            product.minimumStock = minimum;

            service.updateProduct(id, product);
            load();
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not edit product", error.what());
        }
    });

    connect(archiveBtn_, &QPushButton::clicked, this, [this] {
        const int row = table_->currentRow();
        if (row < 0) {
            QMessageBox::information(this, "Archive product", "Select a product first.");
            return;
        }
        const auto id = table_->item(row, 0)->data(Qt::UserRole).toString();
        const auto name = table_->item(row, 1)->text();
        if (QMessageBox::question(this, "Archive product", QString("Archive %1? It will no longer appear in active inventory or sales.").arg(name), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
        try {
            pos::InventoryService(database_).archiveProduct(id);
            load();
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not archive product", error.what());
        }
    });

    connect(printLabelBtn_, &QPushButton::clicked, this, [this] {
        const int row = table_->currentRow();
        if (row < 0) {
            QMessageBox::information(this, "Print label", "Select a product first.");
            return;
        }
        const auto id = table_->item(row, 0)->data(Qt::UserRole).toString();
        try {
            pos::InventoryService service(database_);
            auto query = service.findProduct(id);
            if (query.barcode.trimmed().isEmpty()) {
                QMessageBox::warning(this, "Print label", "The selected product has no barcode.");
                return;
            }
            const auto path = pos::SettingsService(database_).value("printer.thermal_path");
            pos::ThermalPrintService::writeRaw(path, pos::ThermalPrintService::barcodeLabelBytes(query.name, query.barcode));
            QMessageBox::information(this, "Label sent", "The barcode label was sent to the configured thermal device.");
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not print label", error.what());
        }
    });

    connect(importCsvBtn_, &QPushButton::clicked, this, [this] {
        const auto fileName = QFileDialog::getOpenFileName(this, "Import products", {}, "CSV files (*.csv)");
        if (fileName.isEmpty()) return;
        try {
            QFile file(fileName);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) throw pos::DatabaseError("could not open import file");
            QTextStream stream(&file);
            if (stream.atEnd()) throw pos::DatabaseError("CSV file is empty");
            
            const auto parse = [](const QString& line) {
                QStringList fields;
                QString field;
                bool quoted = false;
                for (int i = 0; i < line.size(); ++i) {
                    const auto ch = line.at(i);
                    if (ch == '"') {
                        if (quoted && i + 1 < line.size() && line.at(i + 1) == '"') {
                            field += ch; ++i;
                        } else quoted = !quoted;
                    } else if (ch == ',' && !quoted) {
                        fields.append(field.trimmed());
                        field.clear();
                    } else field += ch;
                }
                if (quoted) throw pos::DatabaseError("CSV contains an unterminated quote");
                fields.append(field.trimmed());
                return fields;
            };

            const auto header = parse(stream.readLine());
            if (header.size() < 4 || header.at(0).compare("name", Qt::CaseInsensitive) != 0 || header.at(1).compare("base_unit", Qt::CaseInsensitive) != 0) {
                throw pos::DatabaseError("CSV header must start with name,base_unit,purchase_price_paisa,retail_price_paisa");
            }

            QList<pos::ProductDefinition> products;
            int lineNumber = 1;
            while (!stream.atEnd()) {
                ++lineNumber;
                const auto fields = parse(stream.readLine());
                if (fields.size() == 1 && fields.first().isEmpty()) continue;
                if (fields.size() < 4) throw pos::DatabaseError(QString("CSV line %1 has fewer than four fields").arg(lineNumber));
                bool purchaseOk = false, retailOk = false;
                const auto purchase = fields.at(2).toLongLong(&purchaseOk);
                const auto retail = fields.at(3).toLongLong(&retailOk);
                if (!purchaseOk || !retailOk) throw pos::DatabaseError(QString("CSV line %1 has invalid prices").arg(lineNumber));
                
                pos::ProductDefinition product;
                product.name = fields.at(0);
                product.baseUnit = fields.at(1);
                product.purchasePrice = purchase;
                product.retailPrice = retail;
                if (fields.size() > 4) product.sku = fields.at(4);
                if (fields.size() > 5) product.barcode = fields.at(5);
                if (fields.size() > 6) {
                    bool ok = false;
                    product.wholesalePrice = fields.at(6).toLongLong(&ok);
                    if (!ok) throw pos::DatabaseError(QString("CSV line %1 has invalid wholesale price").arg(lineNumber));
                }
                if (fields.size() > 7) {
                    bool ok = false;
                    product.dealerPrice = fields.at(7).toLongLong(&ok);
                    if (!ok) throw pos::DatabaseError(QString("CSV line %1 has invalid dealer price").arg(lineNumber));
                }
                if (fields.size() > 8) {
                    bool ok = false;
                    product.minimumStock = fields.at(8).toLongLong(&ok);
                    if (!ok) throw pos::DatabaseError(QString("CSV line %1 has invalid minimum stock").arg(lineNumber));
                }
                if (fields.size() > 9) product.trackBatches = fields.at(9) == "1" || fields.at(9).compare("true", Qt::CaseInsensitive) == 0;
                if (fields.size() > 10) product.trackExpiry = fields.at(10) == "1" || fields.at(10).compare("true", Qt::CaseInsensitive) == 0;
                if (fields.size() > 11) product.description = fields.at(11);
                if (fields.size() > 12) product.imagePath = fields.at(12);
                products.append(product);
            }

            pos::InventoryService(database_).importProducts(products);
            load();
            QMessageBox::information(this, "Import complete", QString("Imported %1 products.").arg(products.size()));
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not import products", error.what());
        }
    });

    // Set tab order
    setTabOrder(search_, table_);
    setTabOrder(table_, addBtn_);
    setTabOrder(addBtn_, editBtn_);
    setTabOrder(editBtn_, archiveBtn_);
    setTabOrder(archiveBtn_, printLabelBtn_);
    setTabOrder(printLabelBtn_, importCsvBtn_);
    setTabOrder(importCsvBtn_, refreshBtn_);
}

void InventoryPage::load() {
    table_->setUpdatesEnabled(false);
    
    // Update stats
    pos::InventoryService service(database_);
    const auto stats = service.stats();
    val1_->setText(QString::number(stats.totalSkus));
    val2_->setText(QString::number(stats.lowStock));
    val3_->setText(QString::number(stats.outOfStock));
    val4_->setText("PKR " + pos::formatPaisa(stats.totalValuation));

    // Load table rows
    table_->setRowCount(0);
    try {
        pos::ProductSearchFilter filter;
        filter.text = search_->text();
        const auto products = service.searchProducts(filter);

        for (const auto& product : products) {
            const int row = table_->rowCount();
            table_->insertRow(row);
            
            auto* skuItem = new QTableWidgetItem(product.sku.isEmpty() ? "—" : product.sku);
            skuItem->setFont(QFont("JetBrains Mono", 9));
            skuItem->setData(Qt::UserRole, product.id);
            table_->setItem(row, 0, skuItem);
            
            table_->setItem(row, 1, new QTableWidgetItem(product.name));
            table_->setItem(row, 2, new QTableWidgetItem(product.categoryName.isEmpty() ? "General" : product.categoryName));
            
            QString stockText;
            QColor stockBg;
            QColor stockFg;
            if (product.stock == 0) {
                stockText = "OUT OF STOCK";
                stockBg = QColor("#ffdad6");
                stockFg = QColor("#ba1a1a");
            } else if (product.stock <= product.minimumStock) {
                stockText = QString("%1 LOW").arg(product.stock);
                stockBg = QColor("#ffe08b");
                stockFg = QColor("#745b00");
            } else {
                stockText = QString("%1 IN STOCK").arg(product.stock);
                stockBg = QColor("#ffdbcd");
                stockFg = QColor("#99461f");
            }
            auto* stockItem = new QTableWidgetItem(stockText);
            stockItem->setTextAlignment(Qt::AlignCenter);
            stockItem->setFont(QFont("JetBrains Mono", 9, QFont::Bold));
            stockItem->setBackground(stockBg);
            stockItem->setForeground(stockFg);
            table_->setItem(row, 3, stockItem);
            
            auto* priceItem = new QTableWidgetItem("PKR " + pos::formatPaisa(product.retailPrice));
            priceItem->setFont(QFont("JetBrains Mono", 9));
            priceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table_->setItem(row, 4, priceItem);
            
            auto* totalValueItem = new QTableWidgetItem("PKR " + pos::formatPaisa(product.stock * product.retailPrice));
            totalValueItem->setFont(QFont("JetBrains Mono", 9));
            totalValueItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table_->setItem(row, 5, totalValueItem);
            
            auto* actionWidget = new QWidget;
            auto* actionLayout = new QHBoxLayout(actionWidget);
            actionLayout->setContentsMargins(2, 0, 2, 0);
            actionLayout->setSpacing(4);
            
            auto* edit = new QPushButton(actionWidget);
            edit->setIcon(QIcon(":/icons/edit.svg"));
            edit->setToolTip("Edit product details");
            edit->setFlat(true);
            edit->setFixedSize(32, 32);
            connect(edit, &QPushButton::clicked, table_, [this, row]() {
                table_->setCurrentCell(row, 0);
                editBtn_->click();
            });
            
            auto* archive = new QPushButton(actionWidget);
            archive->setIcon(QIcon(":/icons/archive.svg"));
            archive->setToolTip("Archive product");
            archive->setFlat(true);
            archive->setFixedSize(32, 32);
            connect(archive, &QPushButton::clicked, table_, [this, row]() {
                table_->setCurrentCell(row, 0);
                archiveBtn_->click();
            });
            
            actionLayout->addWidget(edit);
            actionLayout->addWidget(archive);
            actionLayout->addStretch();
            table_->setCellWidget(row, 6, actionWidget);
            table_->setRowHeight(row, 44);
        }
    } catch (...) {}
    
    const int rowCount = table_->rowCount();
    resultsCount_->setText(QString("Showing %1 results").arg(rowCount));
    paginationText_->setText(QString("Showing 1-%1 of %2 items").arg(rowCount).arg(rowCount));
    pageNum_->setText("Page 1 of 1");
    table_->setUpdatesEnabled(true);
    table_->viewport()->update();
    table_->updateGeometry();
}
