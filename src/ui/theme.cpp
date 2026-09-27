#include "ui/theme.h"
#include <QHash>

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

namespace {
// Linear blend of two colours: base*(1-t) + accent*t.
QColor blend(const QColor& base, const QColor& accent, double t) {
    return QColor(int(base.red()   * (1 - t) + accent.red()   * t),
                  int(base.green() * (1 - t) + accent.green() * t),
                  int(base.blue()  * (1 - t) + accent.blue()  * t));
}
// Each page's distinct accent, grouped like the sidebar.
QColor accentFor(const QString& name) {
    static const QHash<QString, QString> map = {
        {"Sales POS", "#2563EB"}, {"Inventory", "#14B8A6"}, {"Courses", "#4F46E5"},
        {"Purchases", "#16A34A"}, {"Customers", "#0EA5E9"}, {"Suppliers", "#10B981"},
        {"Cash & Shifts", "#F59E0B"}, {"Cheques", "#EA580C"}, {"Reports", "#9333EA"},
        {"Audit log", "#64748B"}, {"Commission Settings", "#E11D48"}, {"Partners", "#EC4899"},
        {"Profit & Commission", "#7C3AED"}, {"Settings", "#6B7280"}, {"Backup & Restore", "#06B6D4"},
    };
    return QColor(map.value(name, "#6B7280"));
}
} // namespace

TileStyle navTile(const QString& pageName) {
    const QColor a = accentFor(pageName);
    TileStyle s;
    if (g_dark) {
        const QColor base("#1E2025");
        s.bg = blend(base, a, 0.16);
        s.bgHover = blend(base, a, 0.26);
        s.bgPressed = blend(base, a, 0.36);
        s.border = blend(base, a, 0.42);
        s.iconChipBg = blend(base, a, 0.30);
        s.icon = blend(a, QColor("#ffffff"), 0.45); // lighten for contrast on dark
        s.name = QColor("#E2E2E9");
        s.shortcutBg = QColor("#33353A");
        s.shortcutText = QColor("#C6C6CC");
    } else {
        const QColor base("#ffffff");
        s.bg = blend(base, a, 0.07);
        s.bgHover = blend(base, a, 0.13);
        s.bgPressed = blend(base, a, 0.20);
        s.border = blend(base, a, 0.30);
        s.iconChipBg = blend(base, a, 0.16);
        s.icon = a.darker(105);
        s.name = QColor("#1a1c1c");
        s.shortcutBg = QColor("#eeeeee");
        s.shortcutText = QColor("#55433b");
    }
    s.focus = a;
    return s;
}

} // namespace pos::theme
