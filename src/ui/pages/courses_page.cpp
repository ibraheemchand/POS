#include "ui/pages/courses_page.h"
#include "ui/pages/page_helper.h"
#include "core/database.h"
#include "core/bundle_service.h"
#include "core/inventory_service.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QSpinBox>
#include <QTimer>

namespace {
bool editBundleDialog(QWidget* parent, std::shared_ptr<pos::Database> database, const QString& title, pos::BundleDefinition& definition) {
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setMinimumWidth(520);
    auto* layout = new QVBoxLayout(&dialog);

    auto* form = new QFormLayout;
    auto* nameEdit = new QLineEdit(definition.name, &dialog);
    auto* gradeEdit = new QLineEdit(definition.gradeLabel, &dialog);
    auto* descEdit = new QLineEdit(definition.description, &dialog);
    form->addRow("Course name", nameEdit);
    form->addRow("Grade / label", gradeEdit);
    form->addRow("Description", descEdit);
    layout->addLayout(form);

    auto* hint = new QLabel("Add every book in this course with its quantity. Selecting the course in Sales POS drops each book into the cart at today's price.", &dialog);
    hint->setObjectName("muted");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* booksTable = new QTableWidget(&dialog);
    booksTable->setColumnCount(3);
    booksTable->setHorizontalHeaderLabels({"Book", "Quantity", ""});
    booksTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    booksTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    booksTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    booksTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(booksTable, 1);

    const auto products = pos::InventoryService(database).listActiveSummaries();

    const auto addBookRow = [&](const QString& productId, qint64 quantity) {
        const int row = booksTable->rowCount();
        booksTable->insertRow(row);

        auto* combo = new QComboBox(booksTable);
        int selectIndex = -1;
        for (int i = 0; i < products.size(); ++i) {
            combo->addItem(products.at(i).name, products.at(i).id);
            if (products.at(i).id == productId) selectIndex = i;
        }
        if (selectIndex >= 0) combo->setCurrentIndex(selectIndex);
        booksTable->setCellWidget(row, 0, combo);

        auto* qty = new QSpinBox(booksTable);
        qty->setRange(1, 1000000);
        qty->setValue(static_cast<int>(quantity > 0 ? quantity : 1));
        booksTable->setCellWidget(row, 1, qty);

        auto* remove = new QPushButton("Remove", booksTable);
        booksTable->setCellWidget(row, 2, remove);
        QObject::connect(remove, &QPushButton::clicked, booksTable, [booksTable, remove] {
            for (int r = 0; r < booksTable->rowCount(); ++r) {
                if (booksTable->cellWidget(r, 2) == remove) {
                    booksTable->removeRow(r);
                    break;
                }
            }
        });
    };

    for (const auto& item : definition.items) addBookRow(item.productId, item.quantity);

    auto* addBookBtn = new QPushButton("Add book", &dialog);
    layout->addWidget(addBookBtn);
    QObject::connect(addBookBtn, &QPushButton::clicked, &dialog, [&] {
        if (products.isEmpty()) {
            QMessageBox::warning(&dialog, "No products", "Add products to inventory before building a course.");
            return;
        }
        addBookRow(products.first().id, 1);
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) return false;

    definition.name = nameEdit->text();
    definition.gradeLabel = gradeEdit->text();
    definition.description = descEdit->text();
    definition.items.clear();
    for (int row = 0; row < booksTable->rowCount(); ++row) {
        auto* combo = qobject_cast<QComboBox*>(booksTable->cellWidget(row, 0));
        auto* qty = qobject_cast<QSpinBox*>(booksTable->cellWidget(row, 1));
        if (!combo || !qty || combo->currentData().toString().isEmpty()) continue;
        definition.items.append({combo->currentData().toString(), qty->value()});
    }
    return true;
}
} // namespace

CoursesPage::CoursesPage(std::shared_ptr<pos::Database> database, QWidget* parent)
    : QWidget(parent), database_(database) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* title = new QLabel("Courses", this);
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    auto* subtitle = new QLabel("Build grade/course bundles once — the salesman then loads the whole book list into a sale in one click.", this);
    subtitle->setObjectName("muted");
    layout->addWidget(subtitle);

    auto* controls = new QHBoxLayout;
    auto* searchLabel = new QLabel("Find course:", this);
    search_ = new QLineEdit(this);
    search_->setPlaceholderText("Search course or grade label...");
    search_->setMinimumWidth(280);
    search_->setMaximumWidth(400);
    searchLabel->setBuddy(search_);

    addBtn_ = new QPushButton("&Add course", this);
    addBtn_->setObjectName("primary");
    editBtn_ = new QPushButton("&Edit selected", this);
    archiveBtn_ = new QPushButton("Ar&chive selected", this);
    archiveBtn_->setObjectName("danger");
    refreshBtn_ = new QPushButton("&Refresh", this);

    controls->addWidget(searchLabel);
    controls->addWidget(search_);
    controls->addStretch();
    controls->addWidget(addBtn_);
    controls->addWidget(editBtn_);
    controls->addWidget(archiveBtn_);
    controls->addWidget(refreshBtn_);
    layout->addLayout(controls);

    auto* tablePanel = new QFrame(this);
    tablePanel->setObjectName("panel");
    auto* tableLayout = new QVBoxLayout(tablePanel);
    tableLayout->setContentsMargins(16, 16, 16, 16);

    table_ = new QTableWidget(tablePanel);
    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels({"Course name", "Grade / label", "Books"});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    tableLayout->addWidget(table_, 1);
    layout->addWidget(tablePanel, 1);

    feedbackLabel_ = new QLabel(this);
    feedbackLabel_->setObjectName("muted");
    layout->addWidget(feedbackLabel_);

    QTimer::singleShot(0, this, [this] { load(); });
    connect(search_, &QLineEdit::textChanged, this, [this] { load(); });
    connect(refreshBtn_, &QPushButton::clicked, this, [this] { load(); });
    connect(table_, &QTableWidget::activated, this, [this](const QModelIndex&) { editBtn_->click(); });

    connect(addBtn_, &QPushButton::clicked, this, [this] {
        pos::BundleDefinition definition;
        if (!editBundleDialog(this, database_, "New course", definition)) return;
        try {
            pos::BundleService(database_).createBundle(definition);
            feedbackLabel_->setText(QString("Course \"%1\" created.").arg(definition.name));
            load();
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not create course", error.what());
        }
    });

    connect(editBtn_, &QPushButton::clicked, this, [this] {
        const int row = table_->currentRow();
        if (row < 0) {
            QMessageBox::information(this, "Edit course", "Select a course first.");
            return;
        }
        const auto id = table_->item(row, 0)->data(Qt::UserRole).toString();
        try {
            auto definition = pos::BundleService(database_).findBundle(id);
            if (!editBundleDialog(this, database_, "Edit course", definition)) return;
            pos::BundleService(database_).updateBundle(id, definition);
            feedbackLabel_->setText(QString("Course \"%1\" updated.").arg(definition.name));
            load();
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not edit course", error.what());
        }
    });

    connect(archiveBtn_, &QPushButton::clicked, this, [this] {
        const int row = table_->currentRow();
        if (row < 0) {
            QMessageBox::information(this, "Archive course", "Select a course first.");
            return;
        }
        const auto id = table_->item(row, 0)->data(Qt::UserRole).toString();
        const auto name = table_->item(row, 0)->text();
        if (QMessageBox::question(this, "Archive course", QString("Archive \"%1\"? It will no longer appear in Sales POS.").arg(name), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
        try {
            pos::BundleService(database_).archiveBundle(id);
            feedbackLabel_->setText(QString("Course \"%1\" archived.").arg(name));
            load();
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not archive course", error.what());
        }
    });
}

void CoursesPage::load() {
    table_->setRowCount(0);
    try {
        const auto term = search_->text().trimmed();
        const auto bundles = pos::BundleService(database_).listBundles();
        for (const auto& bundle : bundles) {
            if (!term.isEmpty() && !bundle.name.contains(term, Qt::CaseInsensitive) && !bundle.gradeLabel.contains(term, Qt::CaseInsensitive)) continue;
            const int row = table_->rowCount();
            table_->insertRow(row);
            auto* nameItem = new QTableWidgetItem(bundle.name);
            nameItem->setData(Qt::UserRole, bundle.id);
            table_->setItem(row, 0, nameItem);
            table_->setItem(row, 1, new QTableWidgetItem(bundle.gradeLabel));
            table_->setItem(row, 2, new QTableWidgetItem(QString::number(bundle.itemCount)));
        }
    } catch (...) {}
}
