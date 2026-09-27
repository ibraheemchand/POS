#include "ui/discount_slider.h"
#include "ui/theme.h"
#include <QPainter>
#include <QStyleOptionSlider>

DiscountSlider::DiscountSlider(QWidget* parent) : QSlider(Qt::Horizontal, parent) {
    setMinimum(0);
}

void DiscountSlider::setAllowed(qint64 allowed) {
    allowed_ = allowed;
    update();
}

void DiscountSlider::paintEvent(QPaintEvent*) {
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    const QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
    const QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

    const bool dark = pos::theme::isDark();
    const QColor green = dark ? QColor("#4caf50") : QColor("#2e7d32");
    const QColor red   = dark ? QColor("#ef5350") : QColor("#c62828");
    const QColor handleColor = dark ? QColor("#E2E2E9") : QColor("#ffffff");
    const QColor handleBorder = dark ? QColor("#909096") : QColor("#55433b");

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int trackH = 6;
    const int cy = groove.center().y();
    const QRect track(groove.left(), cy - trackH / 2, groove.width(), trackH);

    p.setPen(Qt::NoPen);
    if (!isEnabled()) {
        // Empty/zero-commission cart: neutral track, no zones.
        p.setBrush(dark ? QColor("#33353A") : QColor("#dadada"));
        p.drawRoundedRect(track, 3, 3);
        return;
    }

    const int span = maximum() - minimum();
    double allowedFrac = span > 0 ? static_cast<double>(allowed_ - minimum()) / span : 1.0;
    allowedFrac = qBound(0.0, allowedFrac, 1.0);
    const int splitX = track.left() + static_cast<int>(track.width() * allowedFrac);

    // Green zone (0 -> allowed).
    p.setBrush(green);
    p.drawRoundedRect(QRect(track.left(), track.top(), splitX - track.left(), track.height()), 3, 3);
    // Red zone (allowed -> max).
    if (splitX < track.right()) {
        p.setBrush(red);
        p.drawRoundedRect(QRect(splitX, track.top(), track.right() - splitX, track.height()), 3, 3);
    }

    // Handle: a filled circle so it reads clearly over both zones.
    const int r = handle.height() / 2;
    p.setBrush(handleColor);
    p.setPen(QPen(handleBorder, 1.5));
    p.drawEllipse(handle.center(), r - 1, r - 1);
}
