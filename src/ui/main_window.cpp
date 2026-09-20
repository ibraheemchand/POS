#include "ui/main_window.h"
#include "core/database.h"
#include "core/pos_service.h"
#include "core/data_change_bus.h"
#include "core/backup_service.h"
#include "core/inventory_service.h"
#include "core/purchase_service.h"
#include "core/customer_service.h"
#include "core/payment_service.h"
#include "core/shift_service.h"
#include "core/supplier_service.h"
#include "core/cheque_service.h"
#include "core/report_service.h"
#include "core/excel_export_service.h"
#include "core/security_service.h"
#include "core/suspended_sale_service.h"
#include "core/audit_service.h"
#include "core/settings_service.h"
#include "core/notification_service.h"
#include "core/thermal_print_service.h"
#include <QApplication>
#include <QDate>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QHash>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QStackedWidget>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTableWidget>
#include <QHeaderView>
#include <QInputDialog>
#include <QSpinBox>
#include <QComboBox>
#include <QDateEdit>
#include <QTime>
#include <QFileDialog>
#include <QFile>
#include <QIcon>
#include <QColor>
#include <QTextStream>
#include <QPrinter>
#include <QPainter>
#include <QLinearGradient>
#include <QBrush>
#include <QPrintDialog>
#include <QVBoxLayout>
#include <QTimer>
#include <QPixmap>
#include <QShortcut>
#include <QKeySequence>

namespace {
QString formatPaisa(pos::Money value) {
    const auto sign = value < 0 ? "-" : "";
    const auto absolute = value < 0 ? -value : value;
    return QString("%1%2.%3").arg(sign).arg(absolute / 100).arg(absolute % 100, 2, 10, QChar('0'));
}

QWidget* pageScroller(QWidget* content) {
    if (!content) return nullptr;
    auto* scroll = new QScrollArea;
    scroll->setObjectName("pageScroll");
    scroll->setWidgetResizable(true);
    scroll->setWidget(content);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    return scroll;
}

const char* lightStyle = R"QSS(
* { font-family: "Manrope", "Segoe UI", "Inter", sans-serif; color: #1a1c1c; font-size: 13px; }
QMainWindow, #content { background: #f9f9f9; }
#sidebar { background: #f3f3f3; min-width: 262px; max-width: 262px; border-right: 1px solid #dbc1b7; }
#brand { color: #99461f; font-weight: 800; font-size: 21px; padding: 16px 22px 2px; letter-spacing: 1px; }
#subtitle { color: #55433b; padding: 0 22px 12px; font-size: 11px; font-weight: 600; }
#navCaption { color: #88726a; padding: 14px 22px 4px; font-size: 10px; font-weight: 800; letter-spacing: 1.1px; }
#workspaceTitle { font-size: 21px; font-weight: 800; color: #1a1c1c; }
#workspaceHint { color: #55433b; font-size: 12px; }
#statusChip { background: #ffdbcd; color: #360f00; border: 1px solid #ffb597; border-radius: 12px; padding: 5px 11px; font-weight: 700; }
#shortcutBar { background: #eeeeee; color: #55433b; border: 1px solid #dbc1b7; border-radius: 8px; padding: 6px 10px; font-size: 11px; font-weight: 600; }
QListWidget { background: transparent; border: 0; color: #55433b; outline: 0; padding: 2px 12px 12px; }
QListWidget::item { border-radius: 10px; padding: 11px 12px; margin: 2px 0; }
QListWidget::item:hover { background: #e8e8e8; color: #99461f; }
QListWidget::item:selected { background: #ffdbcd; color: #360f00; font-weight: 700; }
QListWidget::item:disabled { background: transparent; color: #88726a; font-weight: 800; font-size: 10px; letter-spacing: 1px; padding: 12px; margin: 0; border: 0; }
QLineEdit, QComboBox, QSpinBox, QDateEdit { background: #ffffff; border: 1px solid #dbc1b7; border-radius: 10px; padding: 9px 12px; min-height: 18px; selection-background-color: #ffdbcd; selection-color: #360f00; }
QLineEdit:hover, QComboBox:hover, QSpinBox:hover, QDateEdit:hover { border-color: #88726a; }
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDateEdit:focus { border: 2px solid #99461f; padding: 8px 11px; }
QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled, QDateEdit:disabled { background: #dadada; color: #55433b; }
QComboBox::drop-down, QDateEdit::drop-down, QSpinBox::up-button, QSpinBox::down-button { border: 0; width: 26px; }
QComboBox QAbstractItemView { background: #ffffff; color: #1a1c1c; border: 1px solid #dbc1b7; border-radius: 8px; padding: 4px; selection-background-color: #ffdbcd; selection-color: #360f00; }
QPushButton { border: 1px solid #dbc1b7; border-radius: 10px; min-height: 18px; padding: 9px 15px; background: #ffffff; color: #535f75; font-weight: 700; }
QPushButton:hover { background: #f3f3f3; border-color: #88726a; }
QPushButton:pressed { background: #eeeeee; }
QPushButton:disabled { background: #e8e8e8; color: #dadada; border-color: #dbc1b7; }
QPushButton:focus { border: 2px solid #99461f; padding: 8px 14px; }
#primary { background: #99461f; color: #ffffff; border-color: #99461f; }
#primary:hover { background: #ffdbcd; color: #360f00; border-color: #ffdbcd; }
#primary:pressed { background: #7a3008; border-color: #7a3008; }
#primary:focus { border-color: #ffdbcd; }
#danger { background: #ffdad6; color: #ba1a1a; border-color: #ffdad6; }
#danger:hover { background: #ffb4ab; border-color: #ffb4ab; }
#danger:pressed { background: #93000a; color: #ffffff; border-color: #93000a; }
#danger:disabled { background: #dadada; color: #55433b; border-color: #dbc1b7; }
#danger:focus { border-color: #ba1a1a; }
QTableWidget { background: #ffffff; alternate-background-color: #f9f9f9; border: 1px solid #dbc1b7; border-radius: 12px; gridline-color: #eeeeee; selection-background-color: #ffdbcd; selection-color: #360f00; }
QTableWidget::item { padding: 8px 10px; border-bottom: 1px solid #eeeeee; }
QTableWidget::item:hover { background: #f3f3f3; }
QTableWidget::item:selected { background: #ffdbcd; color: #360f00; }
QHeaderView::section { background: #eeeeee; color: #55433b; border: 0; border-bottom: 1px solid #dbc1b7; padding: 11px 10px; font-size: 11px; font-weight: 800; }
QHeaderView::section:hover { color: #99461f; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 4px; }
QScrollBar::handle:vertical { background: #dadada; border-radius: 5px; min-height: 28px; }
QScrollBar::handle:vertical:hover { background: #88726a; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 4px; }
QScrollBar::handle:horizontal { background: #dadada; border-radius: 5px; min-width: 28px; }
QScrollBar::handle:horizontal:hover { background: #88726a; }
QFrame#metric, QFrame#inventoryMetric, QFrame#panel { background: #ffffff; border: 1px solid #dbc1b7; border-radius: 14px; }
QFrame#metric:hover, QFrame#inventoryMetric:hover, QFrame#panel:hover { border-color: #88726a; }
#metric, #inventoryMetric { min-width: 190px; }
#metricLabel { color: #55433b; font-weight: 700; }
#metricValue { font-family: "JetBrains Mono", monospace; font-size: 18px; font-weight: 800; }
#metricCaption { color: #88726a; font-size: 11px; }
#pageTitle { font-size: 27px; font-weight: 800; color: #1a1c1c; }
#sectionTitle { font-size: 16px; font-weight: 800; color: #1a1c1c; }
#footerCard { background: #eeeeee; border: 1px solid #dbc1b7; border-radius: 12px; margin: 0 12px 6px; }
#footerStore { color: #1a1c1c; font-weight: 750; font-size: 12px; }
#footerStatus { color: #99461f; font-size: 11px; font-weight: 600; }
#quick { background: #ffffff; border: 1px solid #dbc1b7; border-radius: 8px; padding: 5px 6px; font-weight: 700; font-size: 11px; }
#quick:hover { border-color: #99461f; color: #99461f; background: #ffdbcd; }
#quick:pressed { background: #ffb597; }
#quickAccessCaption { color: #88726a; padding: 12px 22px 4px; font-size: 10px; font-weight: 800; letter-spacing: 1.1px; }
#quickGrid { background: transparent; border: 0; padding: 0 14px 4px; }
#sideQuick { background: transparent; border: 1px solid #dbc1b7; border-radius: 8px; padding: 5px 8px; font-weight: 600; font-size: 10.5px; color: #55433b; text-align: left; min-height: 14px; }
#sideQuick:hover { border-color: #99461f; color: #99461f; background: #ffdbcd; }
#sideQuick:pressed { background: #ffb597; }
#recentList { background: transparent; border: 0; }
#recentList::item { border-radius: 8px; padding: 8px 10px; color: #55433b; border-bottom: 1px solid #eeeeee; }
#pageScroll, #pageScroll > QWidget, #pageScroll > QWidget > QWidget { background: transparent; border: 0; }
#posSearch { font-size: 15px; min-height: 22px; border-radius: 12px; padding: 12px 14px; }
#summaryBox { background: #ffffff; border: 1px solid #dbc1b7; border-radius: 12px; padding: 6px 10px; }
#sumLabel { color: #55433b; font-weight: 700; font-size: 11px; }
#sumValue { font-weight: 800; font-size: 13px; color: #1a1c1c; }
#posTotal { font-size: 18px; font-weight: 800; color: #99461f; }
QStatusBar { background: #eeeeee; color: #55433b; border-top: 1px solid #dbc1b7; padding-left: 12px; }
QToolTip { background: #ffffff; color: #1a1c1c; border: 1px solid #dbc1b7; border-radius: 8px; padding: 6px 9px; }
QMessageBox { background: #ffffff; color: #1a1c1c; }
)QSS";

const char* darkStyle = R"QSS(
* { font-family: "Manrope", "Segoe UI", "Inter", sans-serif; color: #E2E2E9; font-size: 13px; }
QMainWindow, #content { background: #111318; } #sidebar { background: #1A1B21; min-width: 262px; max-width: 262px; }
#brand { color: #E9C349; font-weight: 800; font-size: 21px; padding: 16px 22px 2px; letter-spacing: 1px; } #subtitle { color: #C6C6CC; padding: 0 22px 12px; font-size: 11px; font-weight: 600; } #navCaption { color: #909096; padding: 14px 22px 4px; font-size: 10px; font-weight: 800; letter-spacing: 1.1px; }
#workspaceTitle { font-size: 21px; font-weight: 800; color: #E2E2E9; } #workspaceHint, #muted, #metricLabel, #metricCaption { color: #C6C6CC; } #statusChip { background: #123A30; color: #78D6A7; border: 1px solid #205B49; border-radius: 12px; padding: 5px 11px; font-weight: 700; } #shortcutBar { background: #1A1B21; color: #C6C6CC; border: 1px solid #45464C; border-radius: 8px; padding: 6px 10px; font-size: 11px; font-weight: 600; }
QListWidget { background: transparent; border: 0; color: #C6C6CC; outline: 0; padding: 2px 12px 12px; } QListWidget::item { border-radius: 10px; padding: 11px 12px; margin: 2px 0; } QListWidget::item:hover { background: #1E2025; color: #E2E2E9; } QListWidget::item:selected { background: #33353A; color: #E9C349; font-weight: 700; }
QListWidget::item:disabled { background: transparent; color: #909096; font-weight: 800; font-size: 10px; letter-spacing: 1px; padding: 12px; margin: 0; border: 0; }
QLineEdit, QComboBox, QSpinBox, QDateEdit { background: #1E2025; border: 1px solid #45464C; border-radius: 10px; padding: 9px 12px; min-height: 18px; selection-background-color: #E9C349; selection-color: #241a00; } QLineEdit:hover, QComboBox:hover, QSpinBox:hover, QDateEdit:hover { border-color: #909096; } QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDateEdit:focus { border: 2px solid #E9C349; padding: 8px 11px; } QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled, QDateEdit:disabled { background: #1A1B21; color: #909096; }
QComboBox::drop-down, QDateEdit::drop-down, QSpinBox::up-button, QSpinBox::down-button { border: 0; width: 26px; }
QComboBox QAbstractItemView { background: #1E2025; color: #E2E2E9; border: 1px solid #45464C; border-radius: 8px; padding: 4px; selection-background-color: #E9C349; selection-color: #241a00; }
QPushButton { border: 1px solid #45464C; border-radius: 10px; min-height: 18px; padding: 9px 15px; background: #1E2025; color: #E2E2E9; font-weight: 700; } QPushButton:hover { background: #282A2F; border-color: #909096; } QPushButton:pressed { background: #33353A; } QPushButton:disabled { background: #1A1B21; color: #909096; border-color: #45464C; }
QPushButton:focus { border: 2px solid #E9C349; padding: 8px 14px; }
#primary { background: #E9C349; color: #241a00; border-color: #E9C349; } #primary:hover { background: #ffe088; border-color: #ffe088; } #primary:pressed { background: #af8d11; border-color: #af8d11; } #primary:focus { border-color: #dee2f4; }
#danger { background: #3B1A1A; color: #FFB4AB; border-color: #6E3230; } #danger:hover { background: #4A2321; border-color: #9A4C48; } #danger:pressed { background: #331312; border-color: #7E3B37; } #danger:disabled { background: #1B2636; color: #7C879C; border-color: #2C3A52; } #danger:focus { border-color: #FFB4AB; }
QTableWidget { background: #1E2025; alternate-background-color: #1A1B21; border: 1px solid #45464C; border-radius: 12px; gridline-color: #33353A; selection-background-color: #282A2F; selection-color: #E2E2E9; } QTableWidget::item { padding: 8px 10px; border-bottom: 1px solid #33353A; } QTableWidget::item:hover { background: #282A2F; } QTableWidget::item:selected { background: #282A2F; color: #E2E2E9; }
QHeaderView::section { background: #0C0E13; color: #C6C6CC; border: 0; border-bottom: 1px solid #45464C; padding: 11px 10px; font-size: 11px; font-weight: 800; } QHeaderView::section:hover { color: #E9C349; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 4px; } QScrollBar::handle:vertical { background: #33353A; border-radius: 5px; min-height: 28px; } QScrollBar::handle:vertical:hover { background: #45464C; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 4px; } QScrollBar::handle:horizontal { background: #33353A; border-radius: 5px; min-width: 28px; } QScrollBar::handle:horizontal:hover { background: #45464C; }
QFrame#metric, QFrame#inventoryMetric, QFrame#panel { background: #1E2025; border: 1px solid #45464C; border-radius: 14px; } QFrame#metric:hover, QFrame#inventoryMetric:hover, QFrame#panel:hover { border-color: #909096; } #metric, #inventoryMetric { min-width: 190px; } #metricLabel { font-weight: 700; color: #C6C6CC; } #metricValue { font-family: "JetBrains Mono", monospace; font-size: 18px; font-weight: 800; } #metricCaption { font-size: 11px; color: #C6C6CC; } #pageTitle { font-size: 27px; font-weight: 800; color: #E2E2E9; } #sectionTitle { font-size: 16px; font-weight: 800; color: #E2E2E9; }
#footerCard { background: #1A1B21; border: 1px solid #45464C; border-radius: 12px; margin: 0 12px 6px; } #footerStore { color: #E2E2E9; font-weight: 750; font-size: 12px; } #footerStatus { color: #78D6A7; font-size: 11px; font-weight: 600; }
#quick { background: #1E2025; border: 1px solid #45464C; border-radius: 8px; padding: 5px 6px; font-weight: 700; font-size: 11px; } #quick:hover { border-color: #E9C349; color: #E9C349; background: #282A2F; } #quick:pressed { background: #33353A; }
#quickAccessCaption { color: #909096; padding: 12px 22px 4px; font-size: 10px; font-weight: 800; letter-spacing: 1.1px; }
#quickGrid { background: transparent; border: 0; padding: 0 14px 4px; }
#sideQuick { background: transparent; border: 1px solid #45464C; border-radius: 8px; padding: 5px 8px; font-weight: 600; font-size: 10.5px; color: #C6C6CC; text-align: left; min-height: 14px; }
#sideQuick:hover { border-color: #E9C349; color: #E9C349; background: #282A2F; }
#sideQuick:pressed { background: #33353A; }
#recentList { background: transparent; border: 0; } #recentList::item { border-radius: 8px; padding: 8px 10px; color: #C6C6CC; border-bottom: 1px solid #33353A; }
#pageScroll, #pageScroll > QWidget, #pageScroll > QWidget > QWidget { background: transparent; border: 0; }
#posSearch { font-size: 15px; min-height: 22px; border-radius: 12px; padding: 12px 14px; }
#summaryBox { background: #1E2025; border: 1px solid #45464C; border-radius: 12px; padding: 6px 10px; }
#sumLabel { color: #C6C6CC; font-weight: 700; font-size: 11px; } #sumValue { font-weight: 800; font-size: 13px; color: #E2E2E9; }
#posTotal { font-size: 18px; font-weight: 800; color: #E9C349; }
QStatusBar { background: #0C0E13; color: #C6C6CC; border-top: 1px solid #45464C; padding-left: 12px; }
QToolTip { background: #0C0E13; color: #E2E2E9; border: 1px solid #45464C; border-radius: 8px; padding: 6px 9px; }
QMessageBox { background: #1E2025; color: #E2E2E9; }
)QSS";
}

#include "ui/main_window.h"
#include "core/database.h"
#include "core/settings_service.h"
#include "core/backup_service.h"
#include "ui/pages/main_page.h"
#include "ui/pages/dashboard_page.h"
#include "ui/pages/inventory_page.h"
#include "ui/pages/sales_pos_page.h"
#include "ui/pages/purchases_page.h"
#include "ui/pages/customers_page.h"
#include "ui/pages/suppliers_page.h"
#include "ui/pages/cash_management_page.h"
#include "ui/pages/cheques_page.h"
#include "ui/pages/reports_page.h"
#include "ui/pages/audit_log_page.h"
#include "ui/pages/settings_page.h"
#include "ui/pages/backup_restore_page.h"
#include "core/data_change_bus.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QShortcut>
#include <QMessageBox>
#include <QTimer>
#include <QDateTime>
#include <QStandardPaths>
#include <QScrollArea>
#include <QApplication>
#include <QStyle>

MainWindow::MainWindow(std::shared_ptr<pos::Database> database, QWidget* parent)
    : QMainWindow(parent), database_(std::move(database)) {
    Q_INIT_RESOURCE(resources);
    setWindowTitle("Nexora POS"); 
    setWindowIcon(QIcon(":/branding/app_icon")); 
    setMinimumSize(1180, 720); 
    resize(1600, 900);

    auto* root = new QWidget(this); 
    auto* layout = new QHBoxLayout(root); 
    layout->setContentsMargins(0,0,0,0); 
    layout->setSpacing(0);

    auto* sidebar = new QFrame(root); 
    sidebar->setObjectName("sidebar"); 
    auto* sideLayout = new QVBoxLayout(sidebar); 
    sideLayout->setContentsMargins(0,0,0,18); 
    sideLayout->setSpacing(0);

    auto* logo = new QLabel(sidebar); 
    logo->setAlignment(Qt::AlignCenter); 
    logo->setPixmap(QPixmap(":/branding/logo").scaled(92, 92, Qt::KeepAspectRatio, Qt::SmoothTransformation)); 
    logo->setStyleSheet("background: white; border-radius: 14px; padding: 8px; margin-top: 18px;"); 
    logo->setAccessibleName("Nexora POS logo"); 
    sideLayout->addWidget(logo, 0, Qt::AlignHCenter);

    auto* brand = new QLabel("Nexora POS", sidebar); 
    brand->setObjectName("brand"); 
    auto* sub = new QLabel("Enterprise Edition", sidebar); 
    sub->setObjectName("subtitle"); 
    sideLayout->addWidget(brand); 
    sideLayout->addWidget(sub);

    const QList<QPair<QString,QStringList>> navGroups = {
        {"MAIN", {"Main", "Sales POS", "Inventory"}},
        {"BUSINESS", {"Purchases", "Customers", "Suppliers"}},
        {"FINANCE", {"Cash & Shifts", "Cheques"}},
        {"ANALYTICS", {"Reports", "Audit log"}},
        {"SYSTEM", {"Settings", "Backup & Restore"}}
    };

    const QHash<QString,QIcon> navIcons = {
        {"Main", QIcon(":/icons/dashboard.svg")},
        {"Dashboard", QIcon(":/icons/dashboard.svg")},
        {"Sales POS", QIcon(":/icons/sales.svg")},
        {"Inventory", QIcon(":/icons/inventory.svg")},
        {"Purchases", QIcon(":/icons/purchases.svg")},
        {"Customers", QIcon(":/icons/customers.svg")},
        {"Suppliers", QIcon(":/icons/suppliers.svg")},
        {"Cash & Shifts", QIcon(":/icons/cash.svg")},
        {"Cheques", QIcon(":/icons/cheques.svg")},
        {"Reports", QIcon(":/icons/reports.svg")},
        {"Audit log", QIcon(":/icons/audit.svg")},
        {"Settings", QIcon(":/icons/settings.svg")},
        {"Backup & Restore", QIcon(":/icons/backup.svg")},
    };

    QStringList pageNames; 
    QVector<int> navRowToPage;
    auto* navigation = new QListWidget(sidebar); 
    navigation->setAccessibleName("Primary navigation"); 
    navigation->setToolTip("Choose an operational workspace");

    for (const auto& group : navGroups) { 
        auto* caption = new QListWidgetItem(group.first, navigation); 
        caption->setFlags(Qt::NoItemFlags); 
        caption->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter); 
        navRowToPage.append(-1); 
        for (const auto& name : group.second) { 
            navRowToPage.append(pageNames.size()); 
            pageNames.append(name); 
            auto* item = new QListWidgetItem(navIcons.value(name), name, navigation); 
            item->setSizeHint(QSize(item->sizeHint().width(), 38)); 
        } 
    }

    navigation_ = navigation; 
    pageNames_ = pageNames; 
    navRowToPage_ = navRowToPage;

    navigation->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded); 
    navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); 
    navigation->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); 
    navigation->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); 
    navigation->setUniformItemSizes(true); 
    navigation->setCurrentRow(1); 
    sideLayout->addWidget(navigation, 1);

    auto* footerFrame = new QFrame(sidebar); 
    footerFrame->setObjectName("footerCard"); 
    footerFrame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed); 
    auto* footerLayout = new QVBoxLayout(footerFrame); 
    footerLayout->setContentsMargins(14,12,14,12); 
    footerLayout->setSpacing(4);

    auto* store = new QLabel(pos::SettingsService(database_).value("business.name", "Nexora POS"), footerFrame); 
    store->setObjectName("footerStore"); 
    store->setWordWrap(true);
    
    auto* status = new QLabel("●  Local data • offline ready", footerFrame); 
    status->setObjectName("footerStatus");
    
    footerLayout->addWidget(store); 
    footerLayout->addWidget(status); 
    sideLayout->addWidget(footerFrame);
    layout->addWidget(sidebar); 

    auto* content = new QWidget(root); 
    content->setObjectName("content");
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(24,16,24,10);
    contentLayout->setSpacing(10);

    auto* top = new QHBoxLayout; 
    auto* workspace = new QVBoxLayout; 
    auto* workspaceTitle = new QLabel("Main", content);
    workspaceTitle->setObjectName("workspaceTitle"); 
    auto* workspaceHint = new QLabel("Monitor the health of your operation and move quickly to the next task.", content); 
    workspaceHint->setObjectName("workspaceHint"); 
    workspaceHint->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); 
    workspace->addWidget(workspaceTitle); 
    workspace->addWidget(workspaceHint); 
    top->addLayout(workspace, 1); 

    auto* online = new QLabel("OFFLINE READY", content); 
    online->setObjectName("statusChip"); 
    top->addWidget(online); 

    auto* date = new QLabel(QDate::currentDate().toString("ddd, dd MMM yyyy"), content);
    date->setObjectName("muted");
    top->addWidget(date);

    auto* theme = new QPushButton("Theme", content); 
    theme->setToolTip("Switch between light and dark appearance"); 
    top->addWidget(theme);
    contentLayout->addLayout(top);

    pages_ = new QStackedWidget(content);
    const auto makePage = [this](const QString& n) -> QWidget* {
        QWidget* page = nullptr;
        if (n == "Main") page = new MainPage(database_);
        else if (n == "Dashboard") {
            auto* p = new DashboardPage(database_);
            connect(p, &DashboardPage::requestNavigation, this, &MainWindow::goToPage);
            page = p;
        }
        else if (n == "Inventory") page = new InventoryPage(database_);
        else if (n == "Sales POS") {
            auto* p = new SalesPosPage(database_);
            connect(p, &SalesPosPage::requestNavigation, this, &MainWindow::goToPage);
            page = p;
        }
        else if (n == "Purchases") page = new PurchasesPage(database_);
        else if (n == "Customers") page = new CustomersPage(database_);
        else if (n == "Suppliers") page = new SuppliersPage(database_);
        else if (n == "Cash & Shifts") page = new CashManagementPage(database_);
        else if (n == "Cheques") page = new ChequesPage(database_);
        else if (n == "Reports") page = new ReportsPage(database_);
        else if (n == "Audit log") page = new AuditLogPage(database_);
        else if (n == "Settings") page = new SettingsPage(database_);
        else if (n == "Backup & Restore") page = new BackupRestorePage(database_);
        
        const QStringList fixedViewportPages = {"Main", "Dashboard", "Inventory", "Sales POS", "Purchases", "Customers", "Suppliers", "Cash & Shifts", "Reports", "Cheques"};
        return fixedViewportPages.contains(n) ? page : pageScroller(page);
    };

    for (const auto& name : pageNames) {
        pages_->addWidget(makePage(name));
    }
    contentLayout->addWidget(pages_, 1);

    auto* shortcutBar = new QLabel(content);
    shortcutBar->setObjectName("shortcutBar");
shortcutBar->setText("F2 Main  |  F3 Find Product  |  F4 New Purchase  |  F5 Refresh  |  F9 Payment  |  Ctrl+F Search  |  Esc Cancel");
    shortcutBar->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(shortcutBar);
    layout->addWidget(content, 1);
    setCentralWidget(root);

    const QHash<QString,QString> hints = {
        {"Main", "Monitor the health of your operation and move quickly to the next task."},
        {"Dashboard", "Monitor the health of your operation and move quickly to the next task."},
        {"Sales POS", "Build a sale from product search through payment, without losing your place."},
        {"Inventory", "Find products fast and keep stock levels healthy."},
        {"Purchases", "Receive stock from suppliers and manage payables."},
        {"Customers", "Manage customers, balances and receive payments."},
        {"Suppliers", "Manage suppliers, payables and purchasing history."},
        {"Cash & Shifts", "Track the till, shifts and cash transactions."},
        {"Cheques", "Register received and issued cheques and their status."},
        {"Reports", "Filter, print and export business reports."},
        {"Audit log", "Append-only record of sensitive business actions."},
        {"Settings", "Business identity, currency, printers, security and backup."},
        {"Backup & Restore", "Verified backups and a guided, checksum-safe restore."}
    };

    connect(navigation, &QListWidget::currentRowChanged, this, [this, navigation, workspaceTitle, workspaceHint, hints](int row) {
        if (row < 0 || row >= navRowToPage_.size()) return;
        const int pageIndex = navRowToPage_.value(row);
        if (pageIndex < 0 || pageIndex >= pages_->count()) return;
        pages_->setCurrentIndex(pageIndex);
        const auto* item = navigation->item(row);
        if (!item) return;
        const auto name = pageNames_.value(pageIndex);
        workspaceTitle->setText(name);
        workspaceHint->setText(hints.value(name));
        
        auto* currentPageWidget = pages_->widget(pageIndex);
        if (auto* scroller = qobject_cast<QScrollArea*>(currentPageWidget)) {
            currentPageWidget = scroller->widget();
        }
        if (currentPageWidget) {
            QMetaObject::invokeMethod(currentPageWidget, "load");
        }
    });

    connect(theme, &QPushButton::clicked, this, &MainWindow::switchTheme); 
    qApp->setStyleSheet(lightStyle); 
    dark_ = false; 
    statusBar()->showMessage("Ready — local data is available offline");

    const auto updateShortcutBar = [this, shortcutBar, navigation](int row) {
        if (row < 0 || row >= navRowToPage_.size()) return;
        const int pageIndex = navRowToPage_.value(row);
        if (pageIndex < 0) return;
        const auto page = pageNames_.value(pageIndex);
        if (page == "Sales POS") shortcutBar->setText("F2 New Sale  |  F3 Find Product  |  F5 Refresh  |  Ctrl+S Save  |  Ctrl+P Print  |  Esc Cancel");
        else if (page == "Inventory") shortcutBar->setText("F3 Find Product  |  F5 Refresh  |  Enter Edit  |  Del Archive  |  Ctrl+F Search");
        else if (page == "Purchases") shortcutBar->setText("F4 New Purchase  |  F5 Refresh Lists  |  Ctrl+S Receive  |  Esc Clear");
        else shortcutBar->setText("F2 New Sale  |  F3 Find Product  |  F4 New Purchase  |  F5 Refresh  |  F9 Payment  |  Ctrl+F Search  |  Esc Cancel");
    };

    connect(navigation, &QListWidget::currentRowChanged, this, updateShortcutBar);
    updateShortcutBar(navigation->currentRow());

    auto* initialWidget = pages_->currentWidget();
    if (auto* scroller = qobject_cast<QScrollArea*>(initialWidget)) {
        initialWidget = scroller->widget();
    }
    if (initialWidget) {
        QMetaObject::invokeMethod(initialWidget, "load");
    }

    auto* f2 = new QShortcut(QKeySequence(Qt::Key_F2), this); 
    connect(f2, &QShortcut::activated, this, [this]{ goToPage("Main"); });
    
    auto* f3 = new QShortcut(QKeySequence(Qt::Key_F3), this); 
    connect(f3, &QShortcut::activated, this, [this]{ 
        goToPage("Inventory"); 
        if (auto* search = pages_->findChild<QLineEdit*>("inventorySearch")) search->setFocus();
    });
    
    auto* f4 = new QShortcut(QKeySequence(Qt::Key_F4), this); 
    connect(f4, &QShortcut::activated, this, [this]{ goToPage("Purchases"); });
    
    auto* f5 = new QShortcut(QKeySequence(Qt::Key_F5), this); 
    connect(f5, &QShortcut::activated, this, [this] {
        auto* currentPageWidget = pages_->currentWidget();
        if (auto* scroller = qobject_cast<QScrollArea*>(currentPageWidget)) {
            currentPageWidget = scroller->widget();
        }
        if (currentPageWidget) {
            QMetaObject::invokeMethod(currentPageWidget, "load");
        }
    });

    auto* f9 = new QShortcut(QKeySequence(Qt::Key_F9), this); 
    connect(f9, &QShortcut::activated, this, [this]{ goToPage("Customers"); });
    
    auto* esc = new QShortcut(QKeySequence(Qt::Key_Escape), this); 
    connect(esc, &QShortcut::activated, this, [this]{ 
        if (QMessageBox::question(this, "Cancel", "Cancel the current action and return to Main?") == QMessageBox::Yes)
            goToPage("Main");
    });

    auto* ctrlF = new QShortcut(QKeySequence::Find, this); 
    connect(ctrlF, &QShortcut::activated, this, [this]{ 
        if (auto* search = pages_->findChild<QLineEdit*>("inventorySearch")) {
            goToPage("Inventory");
            search->setFocus();
            search->selectAll();
        } else if (auto* search = pages_->findChild<QLineEdit*>("posSearch")) {
            goToPage("Sales POS");
            search->setFocus();
            search->selectAll();
        } 
    });

    const auto backupIntervalHours = pos::SettingsService(database_).value("backup.interval_hours", "0").toInt();
    if (backupIntervalHours > 0) {
        auto* timer = new QTimer(this);
        timer->setInterval(backupIntervalHours*60*60*1000);
        connect(timer, &QTimer::timeout, this, [this]{
            try {
                const auto folder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/backups";
                const auto file = std::filesystem::path(folder.toStdWString()) / (L"scheduled-" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss").toStdWString() + L".db");
                pos::BackupService service(database_);
                service.createVerifiedBackup(file);
                service.pruneVerifiedBackups(30);
            } catch(...) {}
        });
        timer->start();
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::goToPage(const QString& pageName) { 
    if (!navigation_) return; 
    const int pageIndex = pageNames_.indexOf(pageName); 
    if (pageIndex < 0) return; 
    const int navRow = navRowToPage_.indexOf(pageIndex); 
    if (navRow < 0) return; 
    if (navigation_->currentRow() != navRow) navigation_->setCurrentRow(navRow); 
}

void MainWindow::switchTheme() {
    dark_ = !dark_;
    qApp->setStyleSheet(dark_ ? darkStyle : lightStyle);
}
