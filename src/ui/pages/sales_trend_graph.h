#pragma once

#include <QWidget>
#include <QVector>
#include <QStringList>

class SalesTrendGraph : public QWidget {
    Q_OBJECT
public:
    explicit SalesTrendGraph(QWidget* parent = nullptr);
    void setData(const QVector<double>& values, const QStringList& labels);
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    QVector<double> values_;
    QStringList labels_;
};
