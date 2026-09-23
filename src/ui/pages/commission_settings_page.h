#pragma once

#include <QWidget>
#include <memory>
#include <QTableWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

namespace pos { class Database; }

// Owner-only. Both course and book partner-commission settings live here, behind
// the same PIN as the discount override. Cashiers never see this page.
class CommissionSettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit CommissionSettingsPage(std::shared_ptr<pos::Database> database, QWidget* parent = nullptr);
public slots:
    void load();
private:
    bool ensureUnlocked();
    void editSelected();
    std::shared_ptr<pos::Database> database_;
    QLineEdit* search_{};
    QTableWidget* table_{};
    QPushButton* editBtn_{};
    QPushButton* changePinBtn_{};
    QPushButton* refreshBtn_{};
    QLabel* feedbackLabel_{};
    QLabel* lockedLabel_{};
};
