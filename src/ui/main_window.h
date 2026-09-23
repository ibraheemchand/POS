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
protected:
    void resizeEvent(QResizeEvent* event) override;
private:
    void applySidebarMode(bool collapsed);
    std::shared_ptr<pos::Database> database_;
    QStackedWidget* pages_{};
    QListWidget* navigation_{};
    QStringList pageNames_;
    QVector<int> navRowToPage_;
    bool dark_{false};
    int lastGoodRow_{1};   // row to fall back to when an owner-PIN unlock is cancelled
    bool navGuard_{false}; // guards against re-entrancy when reverting the selection
    // Sidebar collapse-to-icons on narrow windows.
    QWidget* sidebar_{};
    QWidget* brand_{};
    QWidget* subtitle_{};
    QWidget* footerFrame_{};
    QStringList navItemTexts_;
    bool sidebarCollapsed_{false};
};
