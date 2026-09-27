#pragma once

#include <QWidget>
#include <memory>
#include <QVector>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QGridLayout>

namespace pos { class Database; }

class MainPage : public QWidget {
    Q_OBJECT
public:
    explicit MainPage(std::shared_ptr<pos::Database> database, QWidget* parent = nullptr);
public slots:
    void load();
signals:
    void requestNavigation(const QString& pageName);
protected:
    void resizeEvent(QResizeEvent* event) override;
private:
    void relayoutQuickAccess();
    std::shared_ptr<pos::Database> database_;
    QVector<QLabel*> metricValues_;
    QVector<QLabel*> metricCaptions_;
    QListWidget* recent_{};
    QListWidget* lowStockList_{};
    QGridLayout* metricsGrid_{};
    QList<QFrame*> metricCards_;
    int metricColumns_{0};
    QGridLayout* quickGrid_{};
    QList<class QuickTile*> quickButtons_;
    int quickColumns_{0};
};