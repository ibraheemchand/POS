#pragma once
#include "core/types.h"
#include <QColor>
#include <QString>

// Central theme tokens for colours that can't live in the QSS stylesheet because
// they are chosen per-row from data (e.g. a stock badge coloured by stock level).
// The QSS palette itself still lives in main_window.cpp — moving it here is a
// separate follow-up task.
namespace pos::theme {

// The active theme, mirrored from MainWindow so data-driven colours match the
// stylesheet. Defaults to light, so pages constructed outside MainWindow (tests)
// get sensible light shades.
void setDark(bool dark);
bool isDark();

// A stock-level badge: human label plus readable background/foreground for the
// current theme. Green = in stock, amber = low, red = out.
struct StockBadge {
    QString label;
    QColor bg;
    QColor fg;
};
StockBadge stockBadge(Quantity stock, Quantity minimum);

// Accent text colours for the Inventory metric-card counters, theme-aware and
// matched to the badge palette.
QColor lowStockAccent();
QColor outOfStockAccent();

// Per-tile colours for the Main page Quick Access grid. Each page gets its own
// accent; tints are derived over the theme's panel background so light and dark
// both stay readable. The page name stays dark/light (theme text), never coloured.
struct TileStyle {
    QColor bg;           // soft tinted background
    QColor bgHover;      // slightly stronger tint
    QColor bgPressed;    // pressed state
    QColor border;       // matching border
    QColor iconChipBg;   // rounded chip behind the icon
    QColor icon;         // icon colour
    QColor name;         // page-name text (theme default, not the accent)
    QColor shortcutBg;   // grey "key" badge background
    QColor shortcutText; // grey "key" badge text
    QColor focus;        // keyboard focus ring (the accent)
};
TileStyle navTile(const QString& pageName);

} // namespace pos::theme
