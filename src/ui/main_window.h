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
public:
    QString currentPageName() const; // the page the sidebar is currently on
public slots:
    void goToPage(const QString& pageName);
    void switchTheme();
protected:
    void resizeEvent(QResizeEvent* event) override;
private:
    void applyStyle();
    std::shared_ptr<pos::Database> database_;
    QStackedWidget* pages_{};
    QListWidget* navigation_{};
    QStringList pageNames_;
    QVector<int> navRowToPage_;
    bool dark_{false};
    int lastGoodRow_{1};   // row to fall back to when an owner-PIN unlock is cancelled
    bool navGuard_{false}; // guards against re-entrancy when reverting the selection
    // Compact density on narrow windows (tighter paddings + sidebar, same fonts).
    QWidget* sidebar_{};
    bool compact_{false};
    bool densityApplied_{false};
};
