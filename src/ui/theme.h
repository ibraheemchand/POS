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

} // namespace pos::theme
