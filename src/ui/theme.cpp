#include "ui/theme.h"

namespace pos::theme {
namespace {
bool g_dark = false;
} // namespace

void setDark(bool dark) { g_dark = dark; }
bool isDark() { return g_dark; }

StockBadge stockBadge(Quantity stock, Quantity minimum) {
    if (stock == 0) {
        return {"OUT OF STOCK",
                g_dark ? QColor("#4a1512") : QColor("#ffdad6"),
                g_dark ? QColor("#ffb4ab") : QColor("#ba1a1a")};
    }
    if (stock <= minimum) {
        return {QString("%1 LOW").arg(stock),
                g_dark ? QColor("#3a2e05") : QColor("#ffe9b3"),
                g_dark ? QColor("#ffcf5a") : QColor("#7a5300")};
    }
    return {QString("%1 IN STOCK").arg(stock),
            g_dark ? QColor("#14351d") : QColor("#d7f2dd"),
            g_dark ? QColor("#7fdd97") : QColor("#1b5e20")};
}

QColor lowStockAccent() { return g_dark ? QColor("#ffcf5a") : QColor("#7a5300"); }
QColor outOfStockAccent() { return g_dark ? QColor("#ffb4ab") : QColor("#ba1a1a"); }

} // namespace pos::theme
