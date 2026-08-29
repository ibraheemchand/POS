#include "ui/pages/sales_trend_graph.h"
#include <QPainter>
#include <QPaintEvent>
#include <QLinearGradient>

SalesTrendGraph::SalesTrendGraph(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumHeight(180);
}

void SalesTrendGraph::setData(const QVector<double>& values, const QStringList& labels) {
    values_ = values;
    labels_ = labels;
    update();
}

void SalesTrendGraph::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor bgColor = palette().window().color();
    bool isDark = (bgColor.value() < 120);
    QColor textColor = isDark ? QColor("#C6C6CC") : QColor("#55433b");
    QColor titleColor = isDark ? QColor("#E2E2E9") : QColor("#1a1c1c");
    QColor gridColor = isDark ? QColor("#33353A") : QColor("#eeeeee");
    QColor barColorStart = isDark ? QColor("#E9C349") : QColor("#99461f");
    QColor barColorEnd = isDark ? QColor("#f08a5d") : QColor("#ffdbcd");

    int w = width();
    int h = height();
    int paddingLeft = 45;
    int paddingRight = 15;
    int paddingTop = 36;
    int paddingBottom = 26;

    int chartW = w - paddingLeft - paddingRight;
    int chartH = h - paddingTop - paddingBottom;

    painter.setPen(titleColor);
    painter.setFont(QFont("Arial", 9, QFont::Bold));
    painter.drawText(QRect(10, 6, w - 20, 20), Qt::AlignLeft | Qt::AlignVCenter, "Sales Performance (7-Day Trend)");

    if (values_.isEmpty()) {
        painter.setPen(textColor);
        painter.setFont(QFont("Arial", 9));
        painter.drawText(QRect(paddingLeft, paddingTop, chartW, chartH), Qt::AlignCenter, "No sales recorded yet.");
        return;
    }

    double maxVal = 10.0;
    for (double val : values_) {
        if (val > maxVal) maxVal = val;
    }

    painter.setFont(QFont("Arial", 8));
    for (int i = 0; i <= 3; ++i) {
        double val = maxVal * i / 3.0;
        int y = paddingTop + chartH - (chartH * i / 3);
        
        painter.setPen(gridColor);
        painter.drawLine(paddingLeft, y, w - paddingRight, y);
        
        painter.setPen(textColor);
        QString yLabel;
        if (val >= 1000.0) {
            yLabel = QString("%1k").arg(QString::number(val / 1000.0, 'f', 1));
        } else {
            yLabel = QString::number(val, 'f', 0);
        }
        painter.drawText(2, y - 8, paddingLeft - 8, 16, Qt::AlignRight | Qt::AlignVCenter, yLabel);
    }

    int n = values_.size();
    double barSpacing = 8.0;
    double barW = (chartW - (barSpacing * (n - 1))) / n;
    if (barW < 4.0) barW = 4.0;

    for (int i = 0; i < n; ++i) {
        double val = values_[i];
        double ratio = maxVal > 0 ? (val / maxVal) : 0.0;
        double barH = std::max(2.0, chartH * ratio);
        double x = paddingLeft + i * (barW + barSpacing);
        double y = paddingTop + chartH - barH;

        QRectF barRect(x, y, barW, barH);

        QLinearGradient gradient(barRect.topLeft(), barRect.bottomLeft());
        gradient.setColorAt(0.0, barColorStart);
        gradient.setColorAt(1.0, barColorEnd);

        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.drawRoundedRect(barRect, 4.0, 4.0);

        if (val > 0) {
            painter.setPen(titleColor);
            painter.setFont(QFont("Arial", 7, QFont::Bold));
            QString valStr = val >= 1000.0 ? QString("%1k").arg(QString::number(val / 1000.0, 'f', 1)) : QString::number(val, 'f', 0);
            int labelY = std::max(static_cast<int>(y) - 15, paddingTop - 10);
            painter.drawText(QRectF(x - 12, labelY, barW + 24, 14), Qt::AlignCenter, valStr);
        }

        if (i < labels_.size()) {
            painter.setPen(textColor);
            painter.setFont(QFont("Arial", 7));
            painter.drawText(QRectF(x - 12, h - paddingBottom + 4, barW + 24, 18), Qt::AlignCenter, labels_[i]);
        }
    }
}
