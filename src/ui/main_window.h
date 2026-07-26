#pragma once
#include <QMainWindow>
#include <memory>

class QStackedWidget;
class QListWidget;
namespace pos { class Database; class PosService; }

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(std::shared_ptr<pos::Database> database, QWidget* parent=nullptr);
    ~MainWindow() override;
private:
    QWidget* makeDashboard();
    QWidget* makeInventory();
    QWidget* makeSalesPos();
    QWidget* makePurchases();
    QWidget* makeCustomers();
    QWidget* makeBackupRestore();
    QWidget* makePlaceholder(const QString& title, const QString& description);
    QWidget* makeMetric(const QString& label, const QString& value, const QString& caption, const QString& accent);
    void switchTheme();
    std::shared_ptr<pos::Database> database_;
    std::unique_ptr<pos::PosService> pos_;
    QStackedWidget* pages_{};
    bool dark_{false};
};
