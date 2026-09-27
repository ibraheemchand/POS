#pragma once
#include <QFrame>
#include <QString>

class QLabel;

// A colourful Quick Access tile: a tinted, clickable card with an accent icon chip,
// a dark/light page name, and a small grey "key" badge for the shortcut. Built on a
// QFrame (so the child layout sizes correctly); it emits clicked() on mouse/keyboard
// and shows hover/pressed/focus states. The name is a QLabel so "&" shows literally.
class QuickTile : public QFrame {
    Q_OBJECT
public:
    QuickTile(const QString& pageName, const QString& iconPath, const QString& shortcut,
              bool gated, QWidget* parent = nullptr);
    void applyTheme(); // (re)apply the per-tile accent for the current theme

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    void setPressed(bool pressed);
    QString pageName_;
    QString iconPath_;
    QLabel* iconChip_{};
    QLabel* nameLabel_{};
    QLabel* lockLabel_{};
    QLabel* shortcutBadge_{};
    bool gated_{false};
};
