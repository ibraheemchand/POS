#include "ui/pages/backup_restore_page.h"
#include "ui/pages/page_helper.h"
#include "core/database.h"
#include "core/backup_service.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDateTime>
#include <QHeaderView>
#include <QTimer>

BackupRestorePage::BackupRestorePage(std::shared_ptr<pos::Database> database, QWidget* parent)
    : QWidget(parent), database_(database) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);

    auto* title = new QLabel("Backup & Restore", this);
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    auto* explanation = new QLabel("Each backup is a verified, consistent SQLite snapshot. Restoring creates a safety backup of the current database first.", this);
    explanation->setObjectName("muted");
    explanation->setWordWrap(true);
    layout->addWidget(explanation);

    backupBtn_ = new QPushButton("Create verified backup", this);
    backupBtn_->setObjectName("primary");
    
    restoreBtn_ = new QPushButton("Restore selected backup", this);
    restoreBtn_->setObjectName("danger");
    
    restoreExternalBtn_ = new QPushButton("Restore from drive/file", this);
    restoreExternalBtn_->setObjectName("danger");

    auto* actions = new QHBoxLayout;
    actions->addWidget(backupBtn_);
    actions->addWidget(restoreBtn_);
    actions->addWidget(restoreExternalBtn_);
    actions->addStretch();
    layout->addLayout(actions);

    table_ = new QTableWidget(this);
    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels({"Created", "Status", "File"});
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    layout->addWidget(table_, 1);

    // Initial setups
    QTimer::singleShot(0, this, [this] { load(); });

    // Connections
    connect(backupBtn_, &QPushButton::clicked, this, [this] {
        try {
            const auto selected = QFileDialog::getExistingDirectory(this, "Select backup destination (local or USB drive)", QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
            if (selected.isEmpty()) return;
            const auto folder = selected + "/nexora-backups";
            const auto file = std::filesystem::path(folder.toStdWString()) / (L"backup-" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss").toStdWString() + L".db");
            
            pos::BackupService service(database_);
            service.createVerifiedBackup(file);
            service.pruneVerifiedBackups(30);
            load();
            QMessageBox::information(this, "Backup complete", QString("Verified backup saved to:\n%1").arg(QString::fromStdWString(file.wstring())));
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Backup failed", error.what());
        }
    });

    connect(restoreBtn_, &QPushButton::clicked, this, [this] {
        const int row = table_->currentRow();
        if (row < 0) {
            QMessageBox::information(this, "Select a backup", "Select a verified backup to restore.");
            return;
        }
        const auto selected = std::filesystem::path(table_->item(row, 0)->data(Qt::UserRole).toString().toStdWString());
        if (QMessageBox::warning(this, "Restore business data", QString("This replaces the current database with:\n%1\n\nA safety backup of the current database will be created first.").arg(QString::fromStdWString(selected.wstring())), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
        if (!pos::authorizeSensitiveAction(this, database_, "restore business data")) return;
        try {
            const auto safety = std::filesystem::path(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdWString()) / L"backups" / (L"before-restore-" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss").toStdWString() + L".db");
            pos::BackupService service(database_);
            service.restoreVerifiedBackup(selected, safety);
            load();
            QMessageBox::information(this, "Restore complete", "The database passed its integrity check. Restart Nexora POS before taking new sales.");
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Restore failed", error.what());
        }
    });

    connect(restoreExternalBtn_, &QPushButton::clicked, this, [this] {
        const auto fileName = QFileDialog::getOpenFileName(this, "Select verified backup from a drive", {}, "SQLite backups (*.db)");
        if (fileName.isEmpty()) return;
        if (!pos::authorizeSensitiveAction(this, database_, "restore business data")) return;
        const auto selected = std::filesystem::path(fileName.toStdWString());
        if (QMessageBox::warning(this, "Restore business data", QString("Restore this backup?\n%1\n\nA safety backup is created first.").arg(fileName), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
        try {
            const auto safety = std::filesystem::path(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdWString()) / L"backups" / (L"before-restore-" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss").toStdWString() + L".db");
            pos::BackupService(database_).restoreVerifiedBackup(selected, safety);
            load();
            QMessageBox::information(this, "Restore complete", "The selected backup passed integrity verification. Restart Nexora POS before taking new sales.");
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Restore failed", error.what());
        }
    });

    // Set tab order
    setTabOrder(backupBtn_, restoreBtn_);
    setTabOrder(restoreBtn_, restoreExternalBtn_);
    setTabOrder(restoreExternalBtn_, table_);
}

void BackupRestorePage::load() {
    try {
        pos::BackupService service(database_);
        const auto backups = service.verifiedBackups();
        table_->setRowCount(backups.size());
        for (int row = 0; row < backups.size(); ++row) {
            const auto& item = backups[row];
            auto* created = new QTableWidgetItem(item.verifiedAt);
            created->setData(Qt::UserRole, QString::fromStdWString(item.file.wstring()));
            table_->setItem(row, 0, created);
            table_->setItem(row, 1, new QTableWidgetItem(item.status));
            table_->setItem(row, 2, new QTableWidgetItem(QString::fromStdWString(item.file.wstring())));
        }
    } catch (...) {}
}
