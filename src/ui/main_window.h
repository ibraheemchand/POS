#pragma once
#include <QMainWindow>
#include <QStringList>
#include <QVector>
#include <QMultiHash>
#include <functional>
#include <memory>

class QStackedWidget;
class QListWidget;
namespace pos { class Database; class PosService; }

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(std::shared_ptr<pos::Database> database, QWidget* parent=nullptr);
    ~MainWindow() override;
public slots:
    void goToPage(const QString& pageName);
    void switchTheme();
private:
    std::shared_ptr<pos::Database> database_;
    QStackedWidget* pages_{};
    QListWidget* navigation_{};
    QStringList pageNames_;
    QVector<int> navRowToPage_;
    bool dark_{false};
};
