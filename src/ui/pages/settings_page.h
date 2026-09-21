#pragma once

#include <QWidget>
#include <memory>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>

namespace pos { class Database; }

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(std::shared_ptr<pos::Database> database, QWidget* parent = nullptr);
public slots:
    void load();
private:
    void refreshFlexibleLabel();

    std::shared_ptr<pos::Database> database_;

    QLineEdit* businessNameInput_{};
    QLineEdit* phoneInput_{};
    QLineEdit* currencyInput_{};
    QLineEdit* footerInput_{};
    QSpinBox* backupHoursSpin_{};
    QLineEdit* thermalPathInput_{};

    QPushButton* saveSettingsBtn_{};
    QPushButton* setPinBtn_{};
    QPushButton* clearPinBtn_{};
    QPushButton* viewNotificationsBtn_{};
    QPushButton* testReceiptBtn_{};
    QPushButton* testLabelBtn_{};
    QLabel* pinStatusLabel_{};

    QDoubleSpinBox* commissionRateSpin_{};
    QDoubleSpinBox* partnerShareSpin_{};
    QDoubleSpinBox* ownerMinShareSpin_{};
    QLabel* flexibleShareLabel_{};
    QPushButton* saveCommissionBtn_{};
};
