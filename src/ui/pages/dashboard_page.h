#pragma once

#include <QWidget>
#include <memory>
#include <QVector>
#include <QLabel>
#include <QListWidget>

namespace pos { class Database; }
class SalesTrendGraph;

class DashboardPage : public QWidget {
    Q_OBJECT
public:
    explicit DashboardPage(std::shared_ptr<pos::Database> database, QWidget* parent = nullptr);
public slots:
    void load();
signals:
    void requestNavigation(const QString& pageName);
private:
    std::shared_ptr<pos::Database> database_;
    QVector<QLabel*> metricValues_;
    QVector<QLabel*> metricCaptions_;
    SalesTrendGraph* chart_{};
    QListWidget* recent_{};
    QListWidget* lowStockList_{};
};
