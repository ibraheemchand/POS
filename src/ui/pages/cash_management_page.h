#pragma once

#include <QWidget>
#include <memory>
#include <QLabel>
#include <QPushButton>

namespace pos { class Database; }

class CashManagementPage : public QWidget {
    Q_OBJECT
public:
    explicit CashManagementPage(std::shared_ptr<pos::Database> database, QWidget* parent = nullptr);
public slots:
    void load();
private:
    std::shared_ptr<pos::Database> database_;
    QLabel* statusLabel_{};
    QPushButton* openBtn_{};
    QPushButton* closeBtn_{};
};
