#pragma once
#include <QWizard>
#include <memory>

class QLineEdit;
class QCheckBox;
class QComboBox;
namespace pos { class Database; }

// Shown once, on a brand-new (empty) database: business details, owner PIN + recovery
// code, optional printer, and an off-by-default "Load sample data" toggle. Applying
// it seeds nothing unless that toggle is on.
class FirstRunWizard : public QWizard {
    Q_OBJECT
public:
    explicit FirstRunWizard(std::shared_ptr<pos::Database> database, QWidget* parent = nullptr);

    // Perform the setup (PIN, settings, optional sample data) from the current field
    // values. Returns false and fills `error` on failure. Exposed for testing.
    bool applySetup(QString* error);
    QString recoveryCode() const { return recoveryCode_; }

protected:
    void accept() override;
    bool validateCurrentPage() override;

private:
    std::shared_ptr<pos::Database> database_;
    QLineEdit* businessName_{};
    QLineEdit* phone_{};
    QLineEdit* address_{};
    QLineEdit* pin_{};
    QLineEdit* pinConfirm_{};
    QComboBox* printerCombo_{};
    QComboBox* printerModeCombo_{};
    QCheckBox* loadSample_{};
    QString recoveryCode_;
};
