#pragma once

#include <QWidget>
#include <memory>
#include <QTableWidget>
#include <QPushButton>

namespace pos { class Database; }

class BackupRestorePage : public QWidget {
    Q_OBJECT
public:
    explicit BackupRestorePage(std::shared_ptr<pos::Database> database, QWidget* parent = nullptr);
    void load();
private:
    std::shared_ptr<pos::Database> database_;
    QTableWidget* table_{};
    QPushButton* backupBtn_{};
    QPushButton* restoreBtn_{};
    QPushButton* restoreExternalBtn_{};
};
