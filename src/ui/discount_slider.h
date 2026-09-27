#pragma once
#include <QSlider>

// A horizontal slider whose groove is painted in two zones: green from 0 up to the
// "allowed" value (free to use), red beyond it up to the maximum (needs a manager
// PIN). Values are in rupee-hundredths (paisa), same as the money type.
class DiscountSlider : public QSlider {
    Q_OBJECT
public:
    explicit DiscountSlider(QWidget* parent = nullptr);
    void setAllowed(qint64 allowed); // green/red boundary in paisa

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    qint64 allowed_{0};
};
