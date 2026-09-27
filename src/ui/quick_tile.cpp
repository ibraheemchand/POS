#include "ui/quick_tile.h"
#include "ui/theme.h"
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QIcon>
#include <QApplication>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QStyle>

namespace {
// Render a monochrome (currentColor) SVG icon tinted to `color`.
QPixmap tintedIcon(const QString& path, const QColor& color, int px) {
    const qreal dpr = qApp->devicePixelRatio();
    QPixmap base = QIcon(path).pixmap(QSize(px, px), dpr);
    QPixmap out(base.size());
    out.setDevicePixelRatio(base.devicePixelRatio());
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.drawPixmap(0, 0, base);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(out.rect(), color);
    p.end();
    return out;
}
} // namespace

QuickTile::QuickTile(const QString& pageName, const QString& iconPath, const QString& shortcut,
                     bool gated, QWidget* parent)
    : QFrame(parent), pageName_(pageName), iconPath_(iconPath), gated_(gated) {
    setObjectName("quickTile");
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus); // keyboard focus + focus ring
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(11, 6, 11, 6);
    row->setSpacing(10);

    iconChip_ = new QLabel(this);
    iconChip_->setObjectName("tileIconChip");
    iconChip_->setFixedSize(34, 34);
    iconChip_->setAlignment(Qt::AlignCenter);
    row->addWidget(iconChip_, 0, Qt::AlignVCenter);

    auto* col = new QVBoxLayout;
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(3);

    auto* nameRow = new QHBoxLayout;
    nameRow->setContentsMargins(0, 0, 0, 0);
    nameRow->setSpacing(5);
    nameLabel_ = new QLabel(pageName_, this); // QLabel => "&" shown literally, no mnemonic
    nameLabel_->setObjectName("tileName");
    nameRow->addWidget(nameLabel_, 0, Qt::AlignVCenter);
    lockLabel_ = new QLabel(QString::fromUtf8("\xF0\x9F\x94\x92"), this); // lock glyph
    lockLabel_->setObjectName("tileLock");
    lockLabel_->setVisible(gated_);
    nameRow->addWidget(lockLabel_, 0, Qt::AlignVCenter);
    nameRow->addStretch();
    col->addLayout(nameRow);

    auto* keyRow = new QHBoxLayout;
    keyRow->setContentsMargins(0, 0, 0, 0);
    shortcutBadge_ = new QLabel(shortcut, this);
    shortcutBadge_->setObjectName("tileKey");
    keyRow->addWidget(shortcutBadge_, 0, Qt::AlignLeft);
    keyRow->addStretch();
    col->addLayout(keyRow);

    row->addLayout(col, 1);

    for (QWidget* w : {static_cast<QWidget*>(iconChip_), static_cast<QWidget*>(nameLabel_),
                       static_cast<QWidget*>(lockLabel_), static_cast<QWidget*>(shortcutBadge_)})
        w->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    applyTheme();
}

void QuickTile::setPressed(bool pressed) {
    setProperty("pressed", pressed);
    style()->unpolish(this);
    style()->polish(this);
}

void QuickTile::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) { setPressed(true); setFocus(Qt::MouseFocusReason); }
    QFrame::mousePressEvent(e);
}

void QuickTile::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        setPressed(false);
        if (rect().contains(e->pos())) emit clicked();
    }
    QFrame::mouseReleaseEvent(e);
}

void QuickTile::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Space || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        emit clicked();
        return;
    }
    QFrame::keyPressEvent(e);
}

void QuickTile::applyTheme() {
    const auto t = pos::theme::navTile(pageName_);
    setStyleSheet(QString(
        "#quickTile { background:%1; border:1px solid %2; border-radius:12px; }"
        "#quickTile:hover { background:%3; }"
        "#quickTile[pressed=\"true\"] { background:%4; }"
        "#quickTile:focus { border:2px solid %5; }"
        "#tileIconChip { background:%6; border-radius:10px; }"
        "#tileName { color:%7; font-weight:700; font-size:12px; background:transparent; }"
        "#tileLock { font-size:11px; background:transparent; }"
        "#tileKey { background:%8; color:%9; border-radius:5px; padding:1px 6px;"
                 " font-family:'JetBrains Mono','Segoe UI',monospace; font-size:10px; font-weight:700; }")
        .arg(t.bg.name(), t.border.name(), t.bgHover.name(), t.bgPressed.name(), t.focus.name(),
             t.iconChipBg.name(), t.name.name(), t.shortcutBg.name(), t.shortcutText.name()));
    iconChip_->setPixmap(tintedIcon(iconPath_, t.icon, 20));
}
