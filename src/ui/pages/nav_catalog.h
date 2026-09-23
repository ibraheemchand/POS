#pragma once
#include <QString>
#include <QList>

namespace pos {

// Single source of truth for every sidebar page: its display name, sidebar icon,
// keyboard shortcut and whether it is behind the owner PIN. Shared by the sidebar
// shortcuts (main_window), the Main page's Quick Access grid, and the shortcut
// bar so they never drift apart. Order matches the sidebar.
struct NavEntry { QString name; QString iconPath; QString shortcut; bool gated; };

inline QList<NavEntry> navCatalog() {
    return {
        {"Main",                 ":/icons/dashboard.svg", "F2",           false},
        {"Sales POS",            ":/icons/sales.svg",     "Ctrl+1",       false},
        {"Inventory",            ":/icons/inventory.svg", "F3",           false},
        {"Courses",              ":/icons/inventory.svg", "Ctrl+2",       false},
        {"Purchases",            ":/icons/purchases.svg", "F4",           false},
        {"Customers",            ":/icons/customers.svg", "F9",           false},
        {"Suppliers",            ":/icons/suppliers.svg", "Ctrl+3",       false},
        {"Cash & Shifts",        ":/icons/cash.svg",      "Ctrl+4",       false},
        {"Cheques",              ":/icons/cheques.svg",   "Ctrl+5",       false},
        {"Reports",              ":/icons/reports.svg",   "Ctrl+6",       false},
        {"Audit log",            ":/icons/audit.svg",     "Ctrl+7",       false},
        {"Commission Settings",  ":/icons/settings.svg",  "Ctrl+8",       true},
        {"Partners",             ":/icons/customers.svg", "Ctrl+9",       true},
        {"Profit & Commission",  ":/icons/reports.svg",   "Ctrl+0",       true},
        {"Settings",             ":/icons/settings.svg",  "Ctrl+Shift+1", false},
        {"Backup & Restore",     ":/icons/backup.svg",    "Ctrl+Shift+2", false},
    };
}
} // namespace pos
