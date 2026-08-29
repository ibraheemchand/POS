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
QListWidget::item:disabled { background: transparent; color: #88726a; font-weight: 800; font-size: 10px; letter-spacing: 1px; padding: 14px 12px 3px; margin: 0; border: 0; }
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
#metricValue { font-family: "JetBrains Mono", monospace; font-size: 25px; font-weight: 800; }
#metricCaption { color: #88726a; font-size: 11px; }
#pageTitle { font-size: 27px; font-weight: 800; color: #1a1c1c; }
#sectionTitle { font-size: 16px; font-weight: 800; color: #1a1c1c; }
#footerCard { background: #eeeeee; border: 1px solid #dbc1b7; border-radius: 12px; margin: 0 12px 6px; }
#footerStore { color: #1a1c1c; font-weight: 750; font-size: 12px; }
#footerStatus { color: #99461f; font-size: 11px; font-weight: 600; }
#quick { background: #ffffff; border: 1px solid #dbc1b7; border-radius: 12px; padding: 12px 14px; font-weight: 700; }
#quick:hover { border-color: #99461f; color: #99461f; background: #ffdbcd; }
#quick:pressed { background: #ffb597; }
#recentList { background: transparent; border: 0; }
#recentList::item { border-radius: 8px; padding: 8px 10px; color: #55433b; border-bottom: 1px solid #eeeeee; }
#pageScroll, #pageScroll > QWidget, #pageScroll > QWidget > QWidget { background: transparent; border: 0; }
#posSearch { font-size: 15px; min-height: 22px; border-radius: 12px; padding: 12px 14px; }
#summaryBox { background: #ffffff; border: 1px solid #dbc1b7; border-radius: 12px; }
#sumLabel { color: #55433b; font-weight: 700; }
#sumValue { font-weight: 800; font-size: 14px; color: #1a1c1c; }
#posTotal { font-size: 28px; font-weight: 800; color: #99461f; }
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
QListWidget::item:disabled { background: transparent; color: #909096; font-weight: 800; font-size: 10px; letter-spacing: 1px; padding: 14px 12px 3px; margin: 0; border: 0; }
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
QFrame#metric, QFrame#inventoryMetric, QFrame#panel { background: #1E2025; border: 1px solid #45464C; border-radius: 14px; } QFrame#metric:hover, QFrame#inventoryMetric:hover, QFrame#panel:hover { border-color: #909096; } #metric, #inventoryMetric { min-width: 190px; } #metricLabel { font-weight: 700; color: #C6C6CC; } #metricValue { font-family: "JetBrains Mono", monospace; font-size: 25px; font-weight: 800; } #metricCaption { font-size: 11px; color: #C6C6CC; } #pageTitle { font-size: 27px; font-weight: 800; color: #E2E2E9; } #sectionTitle { font-size: 16px; font-weight: 800; color: #E2E2E9; }
#footerCard { background: #1A1B21; border: 1px solid #45464C; border-radius: 12px; margin: 0 12px 6px; } #footerStore { color: #E2E2E9; font-weight: 750; font-size: 12px; } #footerStatus { color: #78D6A7; font-size: 11px; font-weight: 600; }
#quick { background: #1E2025; border: 1px solid #45464C; border-radius: 12px; padding: 12px 14px; font-weight: 700; } #quick:hover { border-color: #E9C349; color: #E9C349; background: #282A2F; } #quick:pressed { background: #33353A; }
#recentList { background: transparent; border: 0; } #recentList::item { border-radius: 8px; padding: 8px 10px; color: #C6C6CC; border-bottom: 1px solid #33353A; }
#pageScroll, #pageScroll > QWidget, #pageScroll > QWidget > QWidget { background: transparent; border: 0; }
#posSearch { font-size: 15px; min-height: 22px; border-radius: 12px; padding: 12px 14px; }
#summaryBox { background: #1E2025; border: 1px solid #45464C; border-radius: 12px; }
#sumLabel { color: #C6C6CC; font-weight: 700; } #sumValue { font-weight: 800; font-size: 14px; color: #E2E2E9; }
#posTotal { font-size: 28px; font-weight: 800; color: #E9C349; }
QStatusBar { background: #0C0E13; color: #C6C6CC; border-top: 1px solid #45464C; padding-left: 12px; }
QToolTip { background: #0C0E13; color: #E2E2E9; border: 1px solid #45464C; border-radius: 8px; padding: 6px 9px; }
QMessageBox { background: #1E2025; color: #E2E2E9; }
)QSS";

class SalesTrendGraph : public QWidget {
public:
    explicit SalesTrendGraph(QWidget* parent = nullptr) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMinimumHeight(180);
    }
    void setData(const QVector<double>& values, const QStringList& labels) {
        values_ = values;
        labels_ = labels;
        update();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QColor bgColor = palette().window().color();
        bool isDark = (bgColor.value() < 120);
        QColor textColor = isDark ? QColor("#C6C6CC") : QColor("#55433b");
        QColor titleColor = isDark ? QColor("#E2E2E9") : QColor("#1a1c1c");
        QColor gridColor = isDark ? QColor("#33353A") : QColor("#eeeeee");
        QColor barColorStart = isDark ? QColor("#E9C349") : QColor("#99461f");
        QColor barColorEnd = isDark ? QColor("#f08a5d") : QColor("#ffdbcd");

        int w = width();
        int h = height();
        int paddingLeft = 50;
        int paddingRight = 20;
        int paddingTop = 40;
        int paddingBottom = 30;

        int chartW = w - paddingLeft - paddingRight;
        int chartH = h - paddingTop - paddingBottom;

        painter.setPen(titleColor);
        painter.setFont(QFont("Arial", 10, QFont::Bold));
        painter.drawText(QRect(10, 10, w - 20, 25), Qt::AlignLeft | Qt::AlignVCenter, "Sales Performance (7-Day Trend)");

        if (values_.isEmpty()) {
            painter.setPen(textColor);
            painter.setFont(QFont("Arial", 10));
            painter.drawText(QRect(paddingLeft, paddingTop, chartW, chartH), Qt::AlignCenter, "No sales recorded yet.");
            return;
        }

        double maxVal = 10.0;
        for (double val : values_) {
            if (val > maxVal) maxVal = val;
        }

        painter.setFont(QFont("Arial", 8));
        for (int i = 0; i <= 4; ++i) {
            double val = maxVal * i / 4.0;
            int y = paddingTop + chartH - (chartH * i / 4);
            
            painter.setPen(gridColor);
            painter.drawLine(paddingLeft, y, w - paddingRight, y);
            
            painter.setPen(textColor);
            QString yLabel;
            if (val >= 1000.0) {
                yLabel = QString("%1k").arg(QString::number(val / 1000.0, 'f', 1));
            } else {
                yLabel = QString::number(val, 'f', 0);
            }
            painter.drawText(5, y - 8, paddingLeft - 10, 16, Qt::AlignRight | Qt::AlignVCenter, yLabel);
        }

        int n = values_.size();
        double barSpacing = 16.0;
        double barW = (chartW - (barSpacing * (n - 1))) / n;
        if (barW < 4.0) barW = 4.0;

        for (int i = 0; i < n; ++i) {
            double val = values_[i];
            double ratio = val / maxVal;
            double barH = chartH * ratio;
            double x = paddingLeft + i * (barW + barSpacing);
            double y = paddingTop + chartH - barH;

            QRectF barRect(x, y, barW, barH);

            QLinearGradient gradient(barRect.topLeft(), barRect.bottomLeft());
            gradient.setColorAt(0.0, barColorStart);
            gradient.setColorAt(1.0, barColorEnd);

            painter.setPen(Qt::NoPen);
            painter.setBrush(gradient);
            painter.drawRoundedRect(barRect, 6.0, 6.0);

            if (val > 0) {
                painter.setPen(titleColor);
                painter.setFont(QFont("Arial", 8, QFont::Bold));
                QString valStr = val >= 1000.0 ? QString("%1k").arg(QString::number(val / 1000.0, 'f', 1)) : QString::number(val, 'f', 0);
                painter.drawText(QRectF(x - 10, y - 20, barW + 20, 18), Qt::AlignCenter, valStr);
            }

            if (i < labels_.size()) {
                painter.setPen(textColor);
                painter.setFont(QFont("Arial", 8));
                painter.drawText(QRectF(x - 10, h - paddingBottom + 5, barW + 20, 20), Qt::AlignCenter, labels_[i]);
            }
        }
    }
private:
    QVector<double> values_;
    QStringList labels_;
};

}

MainWindow::MainWindow(std::shared_ptr<pos::Database> database, QWidget* parent)
    : QMainWindow(parent), database_(std::move(database)), pos_(std::make_unique<pos::PosService>(database_)) {
    setWindowTitle("Nexora POS"); setWindowIcon(QIcon(":/branding/app_icon")); setMinimumSize(1180, 720); resize(1600, 900);
    auto* root=new QWidget(this); auto* layout=new QHBoxLayout(root); layout->setContentsMargins(0,0,0,0); layout->setSpacing(0);
    auto* sidebar=new QFrame(root); sidebar->setObjectName("sidebar"); auto* sideLayout=new QVBoxLayout(sidebar); sideLayout->setContentsMargins(0,0,0,18); sideLayout->setSpacing(0);
    auto* logo=new QLabel(sidebar); logo->setAlignment(Qt::AlignCenter); logo->setPixmap(QPixmap(":/branding/logo").scaled(92, 92, Qt::KeepAspectRatio, Qt::SmoothTransformation)); logo->setStyleSheet("background: white; border-radius: 14px; padding: 8px; margin-top: 18px;"); logo->setAccessibleName("Nexora POS logo"); sideLayout->addWidget(logo,0,Qt::AlignHCenter);
    auto* brand=new QLabel("Nexora POS",sidebar); brand->setObjectName("brand"); auto* sub=new QLabel("Enterprise Edition",sidebar); sub->setObjectName("subtitle"); sideLayout->addWidget(brand); sideLayout->addWidget(sub);
    const QList<QPair<QString,QStringList>> navGroups={{"MAIN",{"Dashboard","Sales POS","Inventory"}},{"BUSINESS",{"Purchases","Customers","Suppliers"}},{"FINANCE",{"Cash & Shifts","Cheques"}},{"ANALYTICS",{"Reports","Analytics","Audit log"}},{"SYSTEM",{"Settings","Backup & Restore"}}};
    const QHash<QString,QIcon> navIcons={
        {"Dashboard", QApplication::style()->standardIcon(QStyle::SP_ComputerIcon)},
        {"Sales POS", QApplication::style()->standardIcon(QStyle::SP_DialogYesButton)},
        {"Inventory", QApplication::style()->standardIcon(QStyle::SP_DriveHDIcon)},
        {"Purchases", QApplication::style()->standardIcon(QStyle::SP_ArrowDown)},
        {"Customers", QApplication::style()->standardIcon(QStyle::SP_DirIcon)},
        {"Suppliers", QApplication::style()->standardIcon(QStyle::SP_DirLinkIcon)},
        {"Cash & Shifts", QApplication::style()->standardIcon(QStyle::SP_DialogApplyButton)},
        {"Cheques", QApplication::style()->standardIcon(QStyle::SP_FileIcon)},
        {"Reports", QApplication::style()->standardIcon(QStyle::SP_FileDialogInfoView)},
        {"Analytics", QApplication::style()->standardIcon(QStyle::SP_FileDialogListView)},
        {"Audit log", QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation)},
        {"Settings", QApplication::style()->standardIcon(QStyle::SP_FileDialogContentsView)},
        {"Backup & Restore", QApplication::style()->standardIcon(QStyle::SP_DialogSaveButton)},
    };
    QStringList pageNames; QVector<int> navRowToPage;
    auto* navigation=new QListWidget(sidebar); navigation->setAccessibleName("Primary navigation"); navigation->setToolTip("Choose an operational workspace");
    for(const auto& group:navGroups){ auto* caption=new QListWidgetItem(group.first,navigation); caption->setFlags(Qt::NoItemFlags); caption->setTextAlignment(Qt::AlignLeft|Qt::AlignVCenter); navRowToPage.append(-1); for(const auto& name:group.second){ navRowToPage.append(pageNames.size()); pageNames.append(name); auto* item=new QListWidgetItem(navIcons.value(name), name, navigation); item->setSizeHint(QSize(item->sizeHint().width(), 38)); } }
    navigation_=navigation; pageNames_=pageNames; navRowToPage_=navRowToPage;
    navigation->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded); navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); navigation->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding); navigation->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); navigation->setUniformItemSizes(true); navigation->setCurrentRow(1); sideLayout->addWidget(navigation,1);
    auto* footerFrame=new QFrame(sidebar); footerFrame->setObjectName("footerCard"); footerFrame->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed); auto* footerLayout=new QVBoxLayout(footerFrame); footerLayout->setContentsMargins(14,12,14,12); footerLayout->setSpacing(4);
    auto* store=new QLabel(pos::SettingsService(database_).value("business.name","Nexora POS"),footerFrame); store->setObjectName("footerStore"); store->setWordWrap(true);
    auto* status=new QLabel("●  Local data • offline ready",footerFrame); status->setObjectName("footerStatus");
    footerLayout->addWidget(store); footerLayout->addWidget(status); sideLayout->addWidget(footerFrame);
    layout->addWidget(sidebar); auto* content=new QWidget(root); content->setObjectName("content");auto* contentLayout=new QVBoxLayout(content);contentLayout->setContentsMargins(24,16,24,10);contentLayout->setSpacing(10);
    auto* top=new QHBoxLayout; auto* workspace=new QVBoxLayout; auto* workspaceTitle=new QLabel("Dashboard",content); workspaceTitle->setObjectName("workspaceTitle"); auto* workspaceHint=new QLabel("Monitor the health of your operation and move quickly to the next task.",content); workspaceHint->setObjectName("workspaceHint"); workspaceHint->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); workspace->addWidget(workspaceTitle); workspace->addWidget(workspaceHint); top->addLayout(workspace,1); auto* online=new QLabel("OFFLINE READY",content); online->setObjectName("statusChip"); top->addWidget(online); auto* date=new QLabel(QDate::currentDate().toString("ddd, dd MMM yyyy"),content);date->setObjectName("muted");top->addWidget(date);auto* theme=new QPushButton("Theme",content); theme->setToolTip("Switch between light and dark appearance"); top->addWidget(theme);contentLayout->addLayout(top);
    pages_=new QStackedWidget(content);
    const auto makePage=[this](const QString& n)->QWidget*{
        QWidget* page=nullptr;
        if(n=="Dashboard") page=makeDashboard();
        else if(n=="Inventory") page=makeInventory();
        else if(n=="Sales POS") page=makeSalesPos();
        else if(n=="Purchases") page=makePurchases();
        else if(n=="Customers") page=makeCustomers();
        else if(n=="Suppliers") page=makeSuppliers();
        else if(n=="Cash & Shifts") page=makeCashManagement();
        else if(n=="Cheques") page=makeCheques();
        else if(n=="Reports") page=makeReports();
        else if(n=="Analytics") page=makeAnalytics();
        else if(n=="Audit log") page=makeAuditLog();
        else if(n=="Settings") page=makeSettings();
        else if(n=="Backup & Restore") page=makeBackupRestore();
        else page=makePlaceholder(n,{});
        const QStringList fixedViewportPages{"Dashboard","Inventory","Sales POS","Purchases","Customers","Suppliers","Cash & Shifts","Reports","Analytics"};
        return fixedViewportPages.contains(n) ? page : pageScroller(page);
    };
    for(const auto& name:pageNames) pages_->addWidget(makePage(name));
    contentLayout->addWidget(pages_,1);
    auto* shortcutBar=new QLabel(content);
    shortcutBar->setObjectName("shortcutBar");
    shortcutBar->setText("F2 New Sale  |  F3 Find Product  |  F4 New Purchase  |  F5 Refresh  |  F9 Payment  |  Ctrl+F Search  |  Esc Cancel");
    shortcutBar->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(shortcutBar);layout->addWidget(content,1);setCentralWidget(root);
    const QHash<QString,QString> hints={{"Dashboard","Monitor the health of your operation and move quickly to the next task."},{"Sales POS","Build a sale from product search through payment, without losing your place."},{"Inventory","Find products fast and keep stock levels healthy."},{"Purchases","Receive stock from suppliers and manage payables."},{"Customers","Manage customers, balances and receive payments."},{"Suppliers","Manage suppliers, payables and purchasing history."},{"Cash & Shifts","Track the till, shifts and cash transactions."},{"Cheques","Register received and issued cheques and their status."},{"Reports","Filter, print and export business reports."},{"Analytics","Revenue, profit and inventory trends at a glance."},{"Audit log","Append-only record of sensitive business actions."},{"Settings","Business identity, currency, printers, security and backup."},{"Backup & Restore","Verified backups and a guided, checksum-safe restore."}};
    connect(navigation,&QListWidget::currentRowChanged,this,[this,navigation,workspaceTitle,workspaceHint,hints](int row){
        if(row<0||row>=navRowToPage_.size())return;
        const int pageIndex=navRowToPage_.value(row);
        if(pageIndex<0||pageIndex>=pages_->count())return;
        pages_->setCurrentIndex(pageIndex);
        const auto* item=navigation->item(row);
        if(!item)return;
        const auto name=pageNames_.value(pageIndex);
        workspaceTitle->setText(name);
        workspaceHint->setText(hints.value(name));
        if(name=="Purchases"){
            reloadPurchaseCombos();
        } else if(name=="Sales POS"){
            reloadSalesCustomerCombo();
        }
    });
    connect(theme,&QPushButton::clicked,this,&MainWindow::switchTheme); qApp->setStyleSheet(lightStyle); dark_ = false; statusBar()->showMessage("Ready — local data is available offline");
    const auto updateShortcutBar=[this,shortcutBar,navigation](int row){
        if(row<0||row>=navRowToPage_.size())return;
        const int pageIndex=navRowToPage_.value(row);
        if(pageIndex<0)return;
        const auto page=pageNames_.value(pageIndex);
        if(page=="Sales POS") shortcutBar->setText("F2 New Sale  |  F3 Find Product  |  F5 Refresh  |  Ctrl+S Save  |  Ctrl+P Print  |  Esc Cancel");
        else if(page=="Inventory") shortcutBar->setText("F3 Find Product  |  F5 Refresh  |  Enter Edit  |  Del Archive  |  Ctrl+F Search");
        else if(page=="Purchases") shortcutBar->setText("F4 New Purchase  |  F5 Refresh Lists  |  Ctrl+S Receive  |  Esc Clear");
        else shortcutBar->setText("F2 New Sale  |  F3 Find Product  |  F4 New Purchase  |  F5 Refresh  |  F9 Payment  |  Ctrl+F Search  |  Esc Cancel");
    };
    connect(navigation,&QListWidget::currentRowChanged,this,updateShortcutBar);
    updateShortcutBar(navigation->currentRow());
    auto* f2=new QShortcut(QKeySequence(Qt::Key_F2),this); connect(f2,&QShortcut::activated,this,[this]{goToPage("Sales POS");});
    auto* f3=new QShortcut(QKeySequence(Qt::Key_F3),this); connect(f3,&QShortcut::activated,this,[this]{goToPage("Inventory"); if(auto* search=pages_->findChild<QLineEdit*>("inventorySearch")) search->setFocus();});
    auto* f4=new QShortcut(QKeySequence(Qt::Key_F4),this); connect(f4,&QShortcut::activated,this,[this]{goToPage("Purchases");});
    auto* f5=new QShortcut(QKeySequence(Qt::Key_F5),this); connect(f5,&QShortcut::activated,this,[this]{ for(const auto& reload:dataRefreshCallbacks_.values("inventory")) if(reload) reload(); for(const auto& reload:dataRefreshCallbacks_.values("sales")) if(reload) reload(); });
    auto* f9=new QShortcut(QKeySequence(Qt::Key_F9),this); connect(f9,&QShortcut::activated,this,[this]{goToPage("Customers");});
    auto* esc=new QShortcut(QKeySequence(Qt::Key_Escape),this); connect(esc,&QShortcut::activated,this,[this]{ if(QMessageBox::question(this,"Cancel","Cancel the current action and return to Dashboard?")==QMessageBox::Yes) goToPage("Dashboard"); });
    auto* ctrlF=new QShortcut(QKeySequence::Find,this); connect(ctrlF,&QShortcut::activated,this,[this]{ if(auto* search=pages_->findChild<QLineEdit*>("inventorySearch")){goToPage("Inventory");search->setFocus();search->selectAll();} else if(auto* search=pages_->findChild<QLineEdit*>("posSearch")){goToPage("Sales POS");search->setFocus();search->selectAll();} });
    connectDataChangeBus();
    const auto backupIntervalHours=pos::SettingsService(database_).value("backup.interval_hours","0").toInt();
    if(backupIntervalHours>0){auto* timer=new QTimer(this);timer->setInterval(backupIntervalHours*60*60*1000);connect(timer,&QTimer::timeout,this,[this]{try{const auto folder=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/backups";const auto file=std::filesystem::path(folder.toStdWString())/(L"scheduled-"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss").toStdWString()+L".db");pos::BackupService service(database_);service.createVerifiedBackup(file);service.pruneVerifiedBackups(30);}catch(...){}});timer->start();}
}

MainWindow::~MainWindow() = default;
void MainWindow::registerDataRefresh(const QString& topic, std::function<void()> reload) {
    dataRefreshCallbacks_.insert(topic, std::move(reload));
}
void MainWindow::connectDataChangeBus() {
    auto& bus = pos::DataChangeBus::instance();
    const auto run = [this](const QString& topic) {
        for (const auto& reload : dataRefreshCallbacks_.values(topic)) {
            if (reload) reload();
        }
    };
    connect(&bus, &pos::DataChangeBus::inventoryChanged, this, [run]{ run("inventory"); });
    connect(&bus, &pos::DataChangeBus::salesChanged, this, [run]{ run("sales"); });
    connect(&bus, &pos::DataChangeBus::purchasesChanged, this, [run]{ run("purchases"); });
    connect(&bus, &pos::DataChangeBus::customersChanged, this, [run]{ run("customers"); });
    connect(&bus, &pos::DataChangeBus::suppliersChanged, this, [run]{ run("suppliers"); });
    connect(&bus, &pos::DataChangeBus::cashChanged, this, [run]{ run("cash"); });
}
void MainWindow::reloadPurchaseCombos() {
    auto* supplier = pages_->findChild<QComboBox*>("purchaseSupplier");
    auto* product = pages_->findChild<QComboBox*>("purchaseProduct");
    if (supplier) {
        const auto currentSupplierId = supplier->currentData().toString();
        supplier->clear();
        supplier->setPlaceholderText("Select supplier");
        auto query = database_->prepare("SELECT id,name FROM suppliers WHERE is_archived=0 ORDER BY name");
        int selectIndex = -1;
        int idx = 0;
        while (query.stepRow()) {
            const auto id = query.text(0);
            supplier->addItem(query.text(1), id);
            if (id == currentSupplierId) selectIndex = idx;
            ++idx;
        }
        if (selectIndex >= 0) supplier->setCurrentIndex(selectIndex);
    }
    if (product) {
        const auto currentProductId = product->currentData().toString();
        product->clear();
        product->setPlaceholderText("Select product");
        auto products = database_->prepare("SELECT id,name,base_unit FROM products WHERE is_deleted=0 ORDER BY name");
        int selectIndex = -1;
        int idx = 0;
        while (products.stepRow()) {
            const auto id = products.text(0);
            product->addItem(products.text(1) + " (" + products.text(2) + ")", id);
            if (id == currentProductId) selectIndex = idx;
            ++idx;
        }
        if (selectIndex >= 0) product->setCurrentIndex(selectIndex);
    }
}
void MainWindow::reloadSalesCustomerCombo() {
    auto* customer = pages_->findChild<QComboBox*>("salesCustomer");
    if (!customer) return;
    const auto currentCustomerId = customer->currentData().toString();
    customer->clear();
    customer->addItem("— Walk-in customer —", QString());
    auto customers = database_->prepare("SELECT id,name FROM customers WHERE is_deleted=0 ORDER BY name");
    int selectIndex = 0;
    int idx = 1;
    while (customers.stepRow()) {
        const auto id = customers.text(0);
        customer->addItem(customers.text(1), id);
        if (id == currentCustomerId) selectIndex = idx;
        ++idx;
    }
    customer->setCurrentIndex(selectIndex);
}
void MainWindow::goToPage(const QString& pageName){ if(!navigation_) return; const int pageIndex=pageNames_.indexOf(pageName); if(pageIndex<0) return; const int navRow=navRowToPage_.indexOf(pageIndex); if(navRow<0) return; if(navigation_->currentRow()!=navRow) navigation_->setCurrentRow(navRow); }
bool MainWindow::authorizeSensitiveAction(const QString& action){pos::SecurityService security(database_);if(!security.hasPin()){QMessageBox::warning(this,"PIN required",QString("Configure a security PIN before %1.").arg(action));return false;}bool ok=false;const auto pin=QInputDialog::getText(this,"Security PIN",QString("Enter PIN to %1:").arg(action),QLineEdit::Password,{},&ok);if(!ok||!security.verifyPin(pin)){QMessageBox::warning(this,"Action denied","The security PIN was incorrect.");return false;}return true;}

QWidget* MainWindow::makeMetric(const QString& label,const QString& value,const QString& caption,const QString& accent){auto* card=new QFrame;card->setObjectName("metric");auto* l=new QVBoxLayout(card);l->setContentsMargins(16,15,16,15);l->setSpacing(6);auto* a=new QLabel(label);a->setObjectName("metricLabel");auto* v=new QLabel(value);v->setObjectName("metricValue");v->setStyleSheet("color:"+accent+";");auto* c=new QLabel(caption);c->setObjectName("metricCaption");l->addWidget(a);l->addWidget(v);l->addWidget(c);return card;}
QWidget* MainWindow::makeDashboard(){
    auto* page=new QWidget; auto* l=new QVBoxLayout(page); l->setContentsMargins(0,12,0,0); l->setSpacing(14);
    auto* heading=new QHBoxLayout; auto* title=new QLabel(page); title->setObjectName("pageTitle"); const auto hour=QTime::currentTime().hour(); title->setText(hour<12?"Good morning":hour<17?"Good afternoon":"Good evening");
    auto* intro=new QLabel("Here’s a live view of your wholesale operation.",page); intro->setObjectName("muted"); auto* tx=new QVBoxLayout; tx->addWidget(title); tx->addWidget(intro); heading->addLayout(tx); heading->addStretch(); auto* backup=new QPushButton("Backup now",page); backup->setObjectName("primary"); heading->addWidget(backup); l->addLayout(heading);

    QVector<QLabel*> metricValues(6);
    QVector<QLabel*> metricCaptions(6);
    auto* metrics=new QGridLayout; metrics->setHorizontalSpacing(14); metrics->setVerticalSpacing(14);
    const QList<QString> accents={"#2563EB","#22C55E","#F59E0B","#EF4444","#7C3AED","#EA580C"};
    const QList<QString> labels={"Today's sales","Today's cash","Receivables","Low stock","Today's purchases","Expiring batches"};
    for(int i=0;i<6;++i){
        auto* card=new QFrame; card->setObjectName("metric"); card->setMinimumHeight(108);
        auto* cardLayout=new QVBoxLayout(card); cardLayout->setContentsMargins(16,15,16,15); cardLayout->setSpacing(6);
        auto* label=new QLabel(labels[i],card); label->setObjectName("metricLabel");
        auto* value=new QLabel("—",card); value->setObjectName("metricValue"); value->setStyleSheet("color:"+accents[i]+";");
        auto* caption=new QLabel("",card); caption->setObjectName("metricCaption");
        cardLayout->addWidget(label); cardLayout->addWidget(value); cardLayout->addWidget(caption);
        metricValues[i]=value; metricCaptions[i]=caption;
        metrics->addWidget(card,i/3,i%3);
    }
    l->addLayout(metrics);

    auto* middle=new QHBoxLayout; middle->setSpacing(16);
    auto* quickPanel=new QFrame(page); quickPanel->setObjectName("panel"); auto* ql=new QVBoxLayout(quickPanel); ql->setContentsMargins(20,18,20,18); ql->setSpacing(12);
    auto* qt=new QLabel("Quick actions",quickPanel); qt->setObjectName("sectionTitle"); auto* qhint=new QLabel("Jump straight to the task you need.",quickPanel); qhint->setObjectName("muted");
    ql->addWidget(qt); ql->addWidget(qhint);
    const QStringList actions={"New sale","New purchase","Add product","Add customer","Add supplier","Receive payment","Cash entry","Current stock","Sales register","Purchase register","Receivables due","Reports"};
    const QStringList targets={"Sales POS","Purchases","Inventory","Customers","Suppliers","Customers","Cash & Shifts","Inventory","Reports","Reports","Customers","Reports"};
    auto* grid=new QGridLayout; grid->setSpacing(10); for(int i=0;i<actions.size();++i){ auto* b=new QPushButton(actions[i],quickPanel); b->setObjectName("quick"); const auto target=targets[i]; connect(b,&QPushButton::clicked,this,[this,target]{goToPage(target);}); grid->addWidget(b,i/4,i%4); } ql->addLayout(grid);
    middle->addWidget(quickPanel, 3);

    auto* graphPanel=new QFrame(page); graphPanel->setObjectName("panel"); auto* gl=new QVBoxLayout(graphPanel); gl->setContentsMargins(12,12,12,12);
    auto* chart=new SalesTrendGraph(graphPanel);
    gl->addWidget(chart);
    middle->addWidget(graphPanel, 2);
    l->addLayout(middle);

    auto* lower=new QHBoxLayout; lower->setSpacing(16);
    auto* activity=new QFrame(page); activity->setObjectName("panel"); auto* al=new QVBoxLayout(activity); al->setContentsMargins(20,18,20,18); al->setSpacing(10); auto* at=new QLabel("Recent activity",activity); at->setObjectName("sectionTitle"); al->addWidget(at);
    auto* recent=new QListWidget(activity); recent->setObjectName("recentList"); recent->setFocusPolicy(Qt::NoFocus);
    al->addWidget(recent,1); lower->addWidget(activity,3);

    auto* lowStockAlerts=new QFrame(page); lowStockAlerts->setObjectName("panel"); auto* nl=new QVBoxLayout(lowStockAlerts); nl->setContentsMargins(20,18,20,18); nl->setSpacing(10); auto* nt=new QLabel("Low Stock Alerts",lowStockAlerts); nt->setObjectName("sectionTitle"); nl->addWidget(nt);
    auto* lowStockList=new QListWidget(lowStockAlerts); lowStockList->setObjectName("recentList"); lowStockList->setFocusPolicy(Qt::NoFocus);
    nl->addWidget(lowStockList,1); lower->addWidget(lowStockAlerts,2);
    l->addLayout(lower,1);

    const auto refreshDashboard=[this,metricValues,metricCaptions,chart,recent,lowStockList](){
        const auto today=QDate::currentDate();
        const auto todayStart=today.toString(Qt::ISODate);
        const auto tomorrow=today.addDays(1).toString(Qt::ISODate);
        pos::BusinessSummary todayReport{};
        qint64 receivables=0,lowStock=0,cashToday=0,salesCount=0,expiring=0;
        try {
            todayReport=pos::ReportService(database_).summary(today,today);
            auto r=database_->prepare("SELECT COALESCE(SUM(balance_paisa),0) FROM customers WHERE is_deleted=0"); r.stepRow(); receivables=r.integer(0);
            auto ls=database_->prepare("SELECT COUNT(*) FROM products WHERE is_deleted=0 AND stock_quantity<=minimum_stock"); ls.stepRow(); lowStock=ls.integer(0);
            auto ct=database_->prepare("SELECT COALESCE(SUM(total_paisa),0) FROM sales WHERE status!='voided' AND payment_method='cash' AND created_at>=? AND created_at<?"); ct.bind(1,todayStart); ct.bind(2,tomorrow); ct.stepRow(); cashToday=ct.integer(0);
            auto sc=database_->prepare("SELECT COUNT(*) FROM sales WHERE status!='voided' AND created_at>=? AND created_at<?"); sc.bind(1,todayStart); sc.bind(2,tomorrow); sc.stepRow(); salesCount=sc.integer(0);
            auto ex=database_->prepare("SELECT COUNT(*) FROM batches WHERE quantity_remaining>0 AND expiry_date IS NOT NULL AND expiry_date>=? AND expiry_date<=?"); ex.bind(1,today.toString(Qt::ISODate)); ex.bind(2,today.addDays(30).toString(Qt::ISODate)); ex.stepRow(); expiring=ex.integer(0);
        } catch(...) {}
        metricValues[0]->setText(QString("PKR %1").arg(formatPaisa(todayReport.sales)));
        metricCaptions[0]->setText(salesCount==0?"No sales yet":QString("%1 sale(s) completed").arg(salesCount));
        metricValues[1]->setText(QString("PKR %1").arg(formatPaisa(cashToday)));
        metricCaptions[1]->setText("Cash taken today");
        metricValues[2]->setText(QString("PKR %1").arg(formatPaisa(receivables)));
        metricCaptions[2]->setText("Customer balances outstanding");
        metricValues[3]->setText(QString("%1 items").arg(lowStock));
        metricCaptions[3]->setText(lowStock==0?"Inventory healthy":"Needs attention");
        metricValues[4]->setText(QString("PKR %1").arg(formatPaisa(todayReport.purchases)));
        metricCaptions[4]->setText("Stock received today");
        metricValues[5]->setText(QString("%1 batches").arg(expiring));
        metricCaptions[5]->setText(expiring==0?"Nothing in the next 30 days":"Due within 30 days");
        const auto updated=QTime::currentTime().toString("HH:mm");
        metricCaptions[0]->setText(metricCaptions[0]->text()+" • Updated "+updated);

        QVector<double> salesValues;
        QStringList salesLabels;
        for (int i = 6; i >= 0; --i) {
            const auto d = today.addDays(-i);
            const auto dateStr = d.toString(Qt::ISODate);
            const auto nextDateStr = d.addDays(1).toString(Qt::ISODate);
            qint64 total = 0;
            try {
                auto q = database_->prepare("SELECT COALESCE(SUM(total_paisa),0) FROM sales WHERE status!='voided' AND created_at>=? AND created_at<?");
                q.bind(1, dateStr);
                q.bind(2, nextDateStr);
                q.stepRow();
                total = q.integer(0);
            } catch (...) {}
            salesValues.append(static_cast<double>(total) / 100.0);
            salesLabels.append(d.toString("dd MMM"));
        }
        chart->setData(salesValues, salesLabels);

        recent->clear();
        try {
            auto s=database_->prepare("SELECT invoice_no,total_paisa,created_at FROM sales WHERE status!='voided' ORDER BY created_at DESC LIMIT 4");
            while(s.stepRow()) recent->addItem(QString("%1   •   PKR %2   •   %3").arg(s.text(0),formatPaisa(s.integer(1)),s.text(2).left(16)));
            auto p=database_->prepare("SELECT invoice_no,total_paisa,purchased_at FROM purchases WHERE status='completed' ORDER BY purchased_at DESC LIMIT 4");
            while(p.stepRow()) recent->addItem(QString("%1   •   PKR %2   •   %3").arg(p.text(0),formatPaisa(p.integer(1)),p.text(2).left(16)));
        } catch(...) {}

        lowStockList->clear();
        try {
            auto ls=database_->prepare("SELECT name,stock_quantity,minimum_stock,base_unit FROM products WHERE is_deleted=0 AND stock_quantity<=minimum_stock ORDER BY stock_quantity ASC LIMIT 10");
            int count=0;
            while(ls.stepRow()){
                ++count;
                lowStockList->addItem(QString("⚠  %1   •   %2 / %3 %4 left").arg(ls.text(0)).arg(ls.integer(1)).arg(ls.integer(2)).arg(ls.text(3)));
            }
            if(count==0){
                auto* item=new QListWidgetItem("✓  All stock levels healthy",lowStockList);
                item->setForeground(QColor("#22C55E"));
                lowStockList->addItem(item);
            }
        } catch(...) {}
    };
    refreshDashboard();
    registerDataRefresh("sales", refreshDashboard);
    registerDataRefresh("purchases", refreshDashboard);
    registerDataRefresh("inventory", refreshDashboard);
    registerDataRefresh("customers", refreshDashboard);
    registerDataRefresh("cash", refreshDashboard);

    connect(backup,&QPushButton::clicked,this,[this]{try {const auto folder=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/backups";const auto file=std::filesystem::path(folder.toStdWString())/(L"backup-"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss").toStdWString()+L".db");pos::BackupService service(database_);service.createVerifiedBackup(file);service.pruneVerifiedBackups(30);QMessageBox::information(this,"Backup complete",QString("A verified backup was saved to:\n%1").arg(QString::fromStdWString(file.wstring())));}catch(const std::exception& e){QMessageBox::critical(this,"Backup failed",e.what());}});return page;
}
QWidget* MainWindow::makeInventory(){
    auto* page=new QWidget; auto* layout=new QVBoxLayout(page); layout->setContentsMargins(0,0,0,0); layout->setSpacing(12);
    
    // Page Header & Caption
    auto* headerLayout = new QHBoxLayout;
    auto* titleContainer = new QVBoxLayout;
    auto* title=new QLabel("Inventory",page); title->setObjectName("pageTitle"); titleContainer->addWidget(title);
    auto* subtitle=new QLabel("Manage stock, track SKUs, and monitor valuation across Main Branch.",page); subtitle->setObjectName("muted"); titleContainer->addWidget(subtitle);
    headerLayout->addLayout(titleContainer); headerLayout->addStretch();
    layout->addLayout(headerLayout);

    // Controls Row
    auto* controls=new QHBoxLayout;
    auto* search=new QLineEdit(page); search->setObjectName("inventorySearch"); search->setPlaceholderText("Search product, SKU or barcode...");
    search->setMinimumWidth(280); search->setMaximumWidth(400);
    auto* filterBtn=new QPushButton("Filters",page);
    auto* add=new QPushButton("Add New Item",page); add->setObjectName("primary");
    controls->addWidget(search); controls->addWidget(filterBtn); controls->addWidget(add); controls->addStretch();
    layout->addLayout(controls);

    // Quick Stats Grid Layout
    auto* statsGrid = new QGridLayout;
    statsGrid->setSpacing(20);
    
    auto* card1 = new QFrame(page); card1->setObjectName("metric");
    auto* l1 = new QVBoxLayout(card1);
    auto* lbl1 = new QLabel("TOTAL SKUS",card1); lbl1->setObjectName("metricLabel");
    auto* val1 = new QLabel("0",card1); val1->setObjectName("metricValue");
    auto* cap1 = new QLabel("+0 this week",card1); cap1->setObjectName("metricCaption");
    l1->addWidget(lbl1); l1->addWidget(val1); l1->addWidget(cap1);
    
    auto* card2 = new QFrame(page); card2->setObjectName("metric");
    auto* l2 = new QVBoxLayout(card2);
    auto* lbl2 = new QLabel("LOW STOCK ITEMS",card2); lbl2->setObjectName("metricLabel");
    auto* val2 = new QLabel("0",card2); val2->setObjectName("metricValue"); val2->setStyleSheet("color: #E9C349;");
    auto* cap2 = new QLabel("Requires reorder",card2); cap2->setObjectName("metricCaption");
    l2->addWidget(lbl2); l2->addWidget(val2); l2->addWidget(cap2);
    
    auto* card3 = new QFrame(page); card3->setObjectName("metric");
    auto* l3 = new QVBoxLayout(card3);
    auto* lbl3 = new QLabel("OUT OF STOCK",card3); lbl3->setObjectName("metricLabel");
    auto* val3 = new QLabel("0",card3); val3->setObjectName("metricValue"); val3->setStyleSheet("color: #FFB4AB;");
    auto* cap3 = new QLabel("Critical attention",card3); cap3->setObjectName("metricCaption");
    l3->addWidget(lbl3); l3->addWidget(val3); l3->addWidget(cap3);
    
    auto* card4 = new QFrame(page); card4->setObjectName("metric");
    auto* l4 = new QVBoxLayout(card4);
    auto* lbl4 = new QLabel("TOTAL INVENTORY VALUE",card4); lbl4->setObjectName("metricLabel");
    auto* val4 = new QLabel("PKR 0.00",card4); val4->setObjectName("metricValue");
    auto* cap4 = new QLabel("Based on retail price",card4); cap4->setObjectName("metricCaption");
    l4->addWidget(lbl4); l4->addWidget(val4); l4->addWidget(cap4);
    
    statsGrid->addWidget(card1, 0, 0); statsGrid->addWidget(card2, 0, 1);
    statsGrid->addWidget(card3, 0, 2); statsGrid->addWidget(card4, 0, 3);
    layout->addLayout(statsGrid);

    // High Density Table Panel
    auto* tablePanel = new QFrame(page); tablePanel->setObjectName("panel");
    auto* tableLayout = new QVBoxLayout(tablePanel); tableLayout->setContentsMargins(16,16,16,16); tableLayout->setSpacing(12);
    
    auto* tableToolbar = new QHBoxLayout;
    auto* resultsCount = new QLabel("Showing 0 results",tablePanel); resultsCount->setObjectName("muted");
    auto* edit=new QPushButton("Edit selected",tablePanel);
    auto* archive=new QPushButton("Archive selected",tablePanel); archive->setObjectName("danger");
    auto* printLabel=new QPushButton("Print barcode label",tablePanel);
    auto* importCsv=new QPushButton("Import CSV",tablePanel);
    auto* refresh=new QPushButton("Refresh",tablePanel);
    tableToolbar->addWidget(resultsCount); tableToolbar->addStretch();
    tableToolbar->addWidget(edit); tableToolbar->addWidget(archive);
    tableToolbar->addWidget(printLabel); tableToolbar->addWidget(importCsv); tableToolbar->addWidget(refresh);
    tableLayout->addLayout(tableToolbar);

    auto* table=new QTableWidget(tablePanel);
    table->setObjectName("inventoryTable");
    table->setColumnCount(7);
    table->setHorizontalHeaderLabels({"SKU / CODE","ITEM NAME","CATEGORY","STOCK LEVEL","UNIT PRICE","TOTAL VALUE","ACTIONS"});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    table->setColumnWidth(3, 148);
    table->setColumnWidth(6, 88);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(44);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    tableLayout->addWidget(table,1);

    // Pagination Footer
    auto* paginationRow = new QHBoxLayout;
    auto* paginationText = new QLabel("Showing 1-250 of 250 items",tablePanel); paginationText->setObjectName("muted");
    auto* prevBtn = new QPushButton("◀",tablePanel); prevBtn->setFixedWidth(36);
    auto* pageNum = new QLabel("Page 1 of 1",tablePanel); pageNum->setObjectName("muted");
    auto* nextBtn = new QPushButton("▶",tablePanel); nextBtn->setFixedWidth(36);
    paginationRow->addWidget(paginationText); paginationRow->addStretch();
    paginationRow->addWidget(prevBtn); paginationRow->addWidget(pageNum); paginationRow->addWidget(nextBtn);
    tableLayout->addLayout(paginationRow);
    layout->addWidget(tablePanel,1);

    const auto load=[this,table,search,val1,val2,val3,val4,resultsCount,paginationText,pageNum,edit,archive](){
        table->setUpdatesEnabled(false);
        // Update stats
        qint64 totalSkus=0, lowStock=0, outOfStock=0, totalValuation=0;
        try {
            auto q1 = database_->prepare("SELECT COUNT(*) FROM products WHERE is_deleted=0"); q1.stepRow(); totalSkus = q1.integer(0);
            auto q2 = database_->prepare("SELECT COUNT(*) FROM products WHERE is_deleted=0 AND stock_quantity<=minimum_stock AND stock_quantity>0"); q2.stepRow(); lowStock = q2.integer(0);
            auto q3 = database_->prepare("SELECT COUNT(*) FROM products WHERE is_deleted=0 AND stock_quantity=0"); q3.stepRow(); outOfStock = q3.integer(0);
            auto q4 = database_->prepare("SELECT COALESCE(SUM(stock_quantity * retail_price_paisa), 0) FROM products WHERE is_deleted=0"); q4.stepRow(); totalValuation = q4.integer(0);
        } catch(...) {}
        
        val1->setText(QString::number(totalSkus));
        val2->setText(QString::number(lowStock));
        val3->setText(QString::number(outOfStock));
        val4->setText(QString("PKR %1").arg(formatPaisa(totalValuation)));
        
        // Load table rows
        table->setRowCount(0);
        try {
            auto query = database_->prepare(
                "SELECT p.id, p.name, p.sku, p.retail_price_paisa, p.stock_quantity, p.minimum_stock, c.name "
                "FROM products p "
                "LEFT JOIN categories c ON p.category_id = c.id "
                "WHERE p.is_deleted=0 AND (p.name LIKE ? OR COALESCE(p.sku,'') LIKE ? OR COALESCE(p.barcode,'') LIKE ?) "
                "ORDER BY p.name LIMIT 250"
            );
            const auto term="%"+search->text().trimmed()+"%";
            query.bind(1,term); query.bind(2,term); query.bind(3,term);
            
            while(query.stepRow()){
                const int row=table->rowCount();
                table->insertRow(row);
                
                const auto id = query.text(0);
                const auto name = query.text(1);
                const auto sku = query.text(2);
                const auto price = query.integer(3);
                const auto stock = query.integer(4);
                const auto minimum = query.integer(5);
                const auto categoryName = query.text(6).isEmpty() ? "General" : query.text(6);
                
                auto* skuItem = new QTableWidgetItem(sku.isEmpty() ? "—" : sku);
                skuItem->setFont(QFont("JetBrains Mono", 9));
                skuItem->setData(Qt::UserRole, id);
                table->setItem(row, 0, skuItem);
                
                table->setItem(row, 1, new QTableWidgetItem(name));
                table->setItem(row, 2, new QTableWidgetItem(categoryName));
                
                QString stockText;
                QColor stockBg;
                QColor stockFg;
                if (stock == 0) {
                    stockText = "OUT OF STOCK";
                    stockBg = QColor("#ffdad6");
                    stockFg = QColor("#ba1a1a");
                } else if (stock <= minimum) {
                    stockText = QString("%1 LOW").arg(stock);
                    stockBg = QColor("#ffe08b");
                    stockFg = QColor("#745b00");
                } else {
                    stockText = QString("%1 IN STOCK").arg(stock);
                    stockBg = QColor("#ffdbcd");
                    stockFg = QColor("#99461f");
                }
                auto* stockItem = new QTableWidgetItem(stockText);
                stockItem->setTextAlignment(Qt::AlignCenter);
                stockItem->setFont(QFont("JetBrains Mono", 9, QFont::Bold));
                stockItem->setBackground(stockBg);
                stockItem->setForeground(stockFg);
                table->setItem(row, 3, stockItem);
                
                auto* priceItem = new QTableWidgetItem(QString("PKR %1").arg(formatPaisa(price)));
                priceItem->setFont(QFont("JetBrains Mono", 9));
                priceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(row, 4, priceItem);
                
                auto* totalValueItem = new QTableWidgetItem(QString("PKR %1").arg(formatPaisa(stock * price)));
                totalValueItem->setFont(QFont("JetBrains Mono", 9));
                totalValueItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(row, 5, totalValueItem);
                
                auto* actionWidget = new QWidget;
                auto* actionLayout = new QHBoxLayout(actionWidget);
                actionLayout->setContentsMargins(2,0,2,0);
                actionLayout->setSpacing(4);
                
                auto* editBtn = new QPushButton(actionWidget);
                editBtn->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView));
                editBtn->setToolTip("Edit product details");
                editBtn->setFlat(true);
                editBtn->setFixedSize(32, 32);
                connect(editBtn, &QPushButton::clicked, table, [table, row, edit]() {
                    table->setCurrentCell(row, 0);
                    edit->click();
                });
                
                auto* archiveBtn = new QPushButton(actionWidget);
                archiveBtn->setIcon(QApplication::style()->standardIcon(QStyle::SP_TrashIcon));
                archiveBtn->setToolTip("Archive product");
                archiveBtn->setFlat(true);
                archiveBtn->setFixedSize(32, 32);
                connect(archiveBtn, &QPushButton::clicked, table, [table, row, archive]() {
                    table->setCurrentCell(row, 0);
                    archive->click();
                });
                
                actionLayout->addWidget(editBtn);
                actionLayout->addWidget(archiveBtn);
                actionLayout->addStretch();
                table->setCellWidget(row, 6, actionWidget);
                table->setRowHeight(row, 44);
            }
        } catch(...) {}
        
        const int rowCount = table->rowCount();
        resultsCount->setText(QString("Showing %1 results").arg(rowCount));
        paginationText->setText(QString("Showing 1-%1 of %2 items").arg(rowCount).arg(rowCount));
        pageNum->setText("Page 1 of 1");
        table->setUpdatesEnabled(true);
        table->viewport()->update();
        table->updateGeometry();
    };

    QTimer::singleShot(0, page, load);
    registerDataRefresh("inventory", load);
    connect(search,&QLineEdit::textChanged,this,[load]{load();});
    connect(table,&QTableWidget::activated,this,[edit](const QModelIndex&){edit->click();});
    auto* delShortcut=new QShortcut(QKeySequence::Delete,table); connect(delShortcut,&QShortcut::activated,this,[archive]{archive->click();});
    connect(refresh,&QPushButton::clicked,this,[load]{load();});
    connect(add,&QPushButton::clicked,this,[this,load]{ bool ok=false; const auto name=QInputDialog::getText(this,"New product","Product name:",QLineEdit::Normal,{},&ok); if(!ok||name.trimmed().isEmpty()) return; const auto unit=QInputDialog::getText(this,"New product","Base unit:",QLineEdit::Normal,"piece",&ok); if(!ok) return; const auto retail=QInputDialog::getInt(this,"New product","Retail price (paisa):",0,0,1000000000,1,&ok); if(!ok) return; try{pos::InventoryService inventory(database_); inventory.createProduct(name,unit,0,retail,false); load();}catch(const std::exception& error){QMessageBox::critical(this,"Could not add product",error.what());} });
    connect(edit,&QPushButton::clicked,this,[this,table,load]{const int row=table->currentRow();if(row<0){QMessageBox::information(this,"Edit product","Select a product first.");return;}const auto id=table->item(row,0)->data(Qt::UserRole).toString();auto query=database_->prepare("SELECT name,sku,barcode,category_id,brand_id,description,base_unit,purchase_price_paisa,retail_price_paisa,wholesale_price_paisa,dealer_price_paisa,minimum_stock,track_batches,track_expiry,image_path FROM products WHERE id=? AND is_deleted=0");query.bind(1,id);if(!query.stepRow()){QMessageBox::warning(this,"Edit product","The selected product is no longer available.");load();return;}bool ok=false;const auto name=QInputDialog::getText(this,"Edit product","Product name:",QLineEdit::Normal,query.text(0),&ok);if(!ok)return;const auto unit=QInputDialog::getText(this,"Edit product","Base unit:",QLineEdit::Normal,query.text(6),&ok);if(!ok)return;const auto purchase=QInputDialog::getInt(this,"Edit product","Purchase price (paisa):",query.integer(7),0,1000000000,1,&ok);if(!ok)return;const auto retail=QInputDialog::getInt(this,"Edit product","Retail price (paisa):",query.integer(8),0,1000000000,1,&ok);if(!ok)return;const auto minimum=QInputDialog::getInt(this,"Edit product","Minimum stock:",query.integer(11),0,1000000000,1,&ok);if(!ok)return;pos::ProductDefinition product;product.name=name;product.sku=query.text(1);product.barcode=query.text(2);product.categoryId=query.text(3);product.brandId=query.text(4);product.description=query.text(5);product.baseUnit=unit;product.purchasePrice=purchase;product.retailPrice=retail;product.wholesalePrice=query.integer(9);product.dealerPrice=query.integer(10);product.minimumStock=minimum;product.trackBatches=query.integer(12)!=0;product.trackExpiry=query.integer(13)!=0;product.imagePath=query.text(14);try{pos::InventoryService(database_).updateProduct(id,product);load();}catch(const std::exception& error){QMessageBox::critical(this,"Could not edit product",error.what());}});
    connect(archive,&QPushButton::clicked,this,[this,table,load]{const int row=table->currentRow();if(row<0){QMessageBox::information(this,"Archive product","Select a product first.");return;}const auto id=table->item(row,0)->data(Qt::UserRole).toString();const auto name=table->item(row,1)->text();if(QMessageBox::question(this,"Archive product",QString("Archive %1? It will no longer appear in active inventory or sales.").arg(name),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;try{pos::InventoryService(database_).archiveProduct(id);load();}catch(const std::exception& error){QMessageBox::critical(this,"Could not archive product",error.what());}});
    connect(printLabel,&QPushButton::clicked,this,[this,table]{const int row=table->currentRow();if(row<0){QMessageBox::information(this,"Print label","Select a product first.");return;}const auto id=table->item(row,0)->data(Qt::UserRole).toString();auto query=database_->prepare("SELECT name,barcode FROM products WHERE id=? AND is_deleted=0");query.bind(1,id);if(!query.stepRow()||query.text(1).trimmed().isEmpty()){QMessageBox::warning(this,"Print label","The selected product has no barcode.");return;}try{const auto path=pos::SettingsService(database_).value("printer.thermal_path");pos::ThermalPrintService::writeRaw(path,pos::ThermalPrintService::barcodeLabelBytes(query.text(0),query.text(1)));QMessageBox::information(this,"Label sent","The barcode label was sent to the configured thermal device.");}catch(const std::exception& error){QMessageBox::critical(this,"Could not print label",error.what());}});
    connect(importCsv,&QPushButton::clicked,this,[this,load]{const auto fileName=QFileDialog::getOpenFileName(this,"Import products",{},"CSV files (*.csv)");if(fileName.isEmpty())return;try{QFile file(fileName);if(!file.open(QIODevice::ReadOnly|QIODevice::Text))throw pos::DatabaseError("could not open import file");QTextStream stream(&file);if(stream.atEnd())throw pos::DatabaseError("CSV file is empty");const auto parse=[](const QString& line){QStringList fields;QString field;bool quoted=false;for(int i=0;i<line.size();++i){const auto ch=line.at(i);if(ch=='"'){if(quoted&&i+1<line.size()&&line.at(i+1)=='"'){field+=ch;++i;}else quoted=!quoted;}else if(ch==','&&!quoted){fields.append(field.trimmed());field.clear();}else field+=ch;}if(quoted)throw pos::DatabaseError("CSV contains an unterminated quote");fields.append(field.trimmed());return fields;};const auto header=parse(stream.readLine());if(header.size()<4||header.at(0).compare("name",Qt::CaseInsensitive)!=0||header.at(1).compare("base_unit",Qt::CaseInsensitive)!=0)throw pos::DatabaseError("CSV header must start with name,base_unit,purchase_price_paisa,retail_price_paisa");QList<pos::ProductDefinition> products;int lineNumber=1;while(!stream.atEnd()){++lineNumber;const auto fields=parse(stream.readLine());if(fields.size()==1&&fields.first().isEmpty())continue;if(fields.size()<4)throw pos::DatabaseError(QString("CSV line %1 has fewer than four fields").arg(lineNumber));bool purchaseOk=false,retailOk=false;const auto purchase=fields.at(2).toLongLong(&purchaseOk);const auto retail=fields.at(3).toLongLong(&retailOk);if(!purchaseOk||!retailOk)throw pos::DatabaseError(QString("CSV line %1 has invalid prices").arg(lineNumber));pos::ProductDefinition product;product.name=fields.at(0);product.baseUnit=fields.at(1);product.purchasePrice=purchase;product.retailPrice=retail;if(fields.size()>4)product.sku=fields.at(4);if(fields.size()>5)product.barcode=fields.at(5);if(fields.size()>6){bool ok=false;product.wholesalePrice=fields.at(6).toLongLong(&ok);if(!ok)throw pos::DatabaseError(QString("CSV line %1 has invalid wholesale price").arg(lineNumber));}if(fields.size()>7){bool ok=false;product.dealerPrice=fields.at(7).toLongLong(&ok);if(!ok)throw pos::DatabaseError(QString("CSV line %1 has invalid dealer price").arg(lineNumber));}if(fields.size()>8){bool ok=false;product.minimumStock=fields.at(8).toLongLong(&ok);if(!ok)throw pos::DatabaseError(QString("CSV line %1 has invalid minimum stock").arg(lineNumber));}if(fields.size()>9)product.trackBatches=fields.at(9)=="1"||fields.at(9).compare("true",Qt::CaseInsensitive)==0;if(fields.size()>10)product.trackExpiry=fields.at(10)=="1"||fields.at(10).compare("true",Qt::CaseInsensitive)==0;if(fields.size()>11)product.description=fields.at(11);if(fields.size()>12)product.imagePath=fields.at(12);products.append(product);}const auto ids=pos::InventoryService(database_).importProducts(products);load();QMessageBox::information(this,"Import complete",QString("Imported %1 products.").arg(ids.size()));}catch(const std::exception& error){QMessageBox::critical(this,"Could not import products",error.what());}});
    return page;
}
QWidget* MainWindow::makeSalesPos(){
    auto* page=new QWidget;
    page->setStyleSheet(R"QSS(
        QLineEdit, QComboBox, QSpinBox { padding: 5px 10px; min-height: 20px; border-radius: 8px; }
        QPushButton { padding: 5px 12px; min-height: 20px; border-radius: 8px; }
        QHeaderView::section { padding: 4px 6px; font-size: 11px; }
        QTableWidget::item { padding: 4px 6px; }
    )QSS");
    auto* layout=new QVBoxLayout(page); layout->setContentsMargins(0,0,0,0); layout->setSpacing(10);
    auto* header=new QFrame(page); header->setObjectName("panel"); auto* hl=new QVBoxLayout(header); hl->setContentsMargins(16,10,16,10); hl->setSpacing(8);
    auto* headTitle=new QLabel("New sale",header); headTitle->setObjectName("sectionTitle");
    auto* date=new QLabel(QDate::currentDate().toString("ddd, dd MMM yyyy"),header); date->setObjectName("muted");
    auto* customer=new QComboBox(header); customer->setObjectName("salesCustomer"); customer->setAccessibleName("Customer"); customer->addItem("— Walk-in customer —",QString());
    auto customers=database_->prepare("SELECT id,name FROM customers WHERE is_deleted=0 ORDER BY name"); while(customers.stepRow()) customer->addItem(customers.text(1),customers.text(0));
    auto* method=new QComboBox(header); method->setAccessibleName("Payment type"); method->addItem("Cash","cash"); method->addItem("Credit","credit"); method->addItem("Cheque","cheque"); method->addItem("Mobile wallet","mobile_wallet"); method->addItem("Mixed","mixed");
    auto* headRow=new QHBoxLayout; headRow->addWidget(date); headRow->addSpacing(10); headRow->addWidget(new QLabel("Customer",header)); headRow->addWidget(customer,1); headRow->addSpacing(10); headRow->addWidget(new QLabel("Payment",header)); headRow->addWidget(method,1);
    hl->addWidget(headTitle); hl->addLayout(headRow); layout->addWidget(header);
    auto* columns=new QHBoxLayout; columns->setSpacing(10);
    auto* productPanel=new QFrame(page); productPanel->setObjectName("panel"); auto* productLayout=new QVBoxLayout(productPanel); productLayout->setContentsMargins(14,12,14,12); productLayout->setSpacing(8);
    auto* productHeading=new QLabel("Product finder",productPanel); productHeading->setObjectName("sectionTitle"); auto* productHint=new QLabel("Search or scan — press Enter to add the first match, or double-click a row.",productPanel); productHint->setObjectName("muted");
    auto* search=new QLineEdit(productPanel); search->setObjectName("posSearch"); search->setPlaceholderText("Search product, SKU or scan barcode"); search->setAccessibleName("Product search or barcode input");
    auto* products=new QTableWidget(productPanel); products->setColumnCount(5); products->setHorizontalHeaderLabels({"Product","SKU","Stock","Price","Unit"}); products->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); products->setSelectionBehavior(QAbstractItemView::SelectRows); products->setEditTriggers(QAbstractItemView::NoEditTriggers); products->setAlternatingRowColors(true); products->verticalHeader()->setDefaultSectionSize(28);
    auto* quantity=new QSpinBox(productPanel); quantity->setRange(1,1000000); quantity->setPrefix("Qty: "); quantity->setAccessibleName("Item quantity"); auto* add=new QPushButton("Add to cart",productPanel); add->setObjectName("primary"); auto* addRow=new QHBoxLayout; addRow->addWidget(quantity,1); addRow->addWidget(add,2);
    productLayout->addWidget(productHeading); productLayout->addWidget(productHint); productLayout->addWidget(search); productLayout->addWidget(products,1); productLayout->addLayout(addRow); columns->addWidget(productPanel,3);
    auto* cartPanel=new QFrame(page); cartPanel->setObjectName("panel"); auto* cartLayout=new QVBoxLayout(cartPanel); cartLayout->setContentsMargins(14,12,14,12); cartLayout->setSpacing(8);
    auto* cartHeading=new QLabel("Current sale",cartPanel); cartHeading->setObjectName("sectionTitle"); auto* cartHint=new QLabel("Select a line to adjust its quantity or remove it.",cartPanel); cartHint->setObjectName("muted");
    auto* cart=new QTableWidget(cartPanel); cart->setColumnCount(4); cart->setHorizontalHeaderLabels({"Product","Quantity","Unit price","Line total"}); cart->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); cart->setEditTriggers(QAbstractItemView::NoEditTriggers); cart->setSelectionBehavior(QAbstractItemView::SelectRows); cart->setAlternatingRowColors(true); cart->verticalHeader()->setDefaultSectionSize(28);
    auto* minus=new QPushButton("−1",cartPanel); auto* plus=new QPushButton("+1",cartPanel); auto* remove=new QPushButton("Remove line",cartPanel); remove->setObjectName("danger"); auto* rowActions=new QHBoxLayout; rowActions->addWidget(minus); rowActions->addWidget(plus); rowActions->addWidget(remove); rowActions->addStretch();
    auto* discount=new QSpinBox(cartPanel); discount->setRange(0,1000000000); discount->setPrefix("Invoice discount (paisa): "); discount->setAccessibleName("Invoice discount in paisa");
    auto* summary=new QFrame(cartPanel); summary->setObjectName("summaryBox"); auto* sl=new QVBoxLayout(summary); sl->setContentsMargins(12,8,12,8); sl->setSpacing(4);
    auto* subtotalLabel=new QLabel("Subtotal",summary); subtotalLabel->setObjectName("sumLabel"); auto* subtotalValue=new QLabel("PKR 0.00",summary); subtotalValue->setObjectName("sumValue");
    auto* discountLabel=new QLabel("Discount",summary); discountLabel->setObjectName("sumLabel"); auto* discountValue=new QLabel("PKR 0.00",summary); discountValue->setObjectName("sumValue");
    auto* subRow=new QHBoxLayout; subRow->addWidget(subtotalLabel); subRow->addStretch(); subRow->addWidget(subtotalValue); auto* discRow=new QHBoxLayout; discRow->addWidget(discountLabel); discRow->addStretch(); discRow->addWidget(discountValue);
    auto* total=new QLabel("PKR 0.00",summary); total->setObjectName("posTotal"); total->setAlignment(Qt::AlignRight);
    sl->addLayout(subRow); sl->addLayout(discRow); sl->addSpacing(2); sl->addWidget(total);
    auto* received=new QSpinBox(cartPanel); received->setRange(0,2147483647); received->setPrefix("Amount received (paisa): "); received->setAccessibleName("Amount received in paisa");
    auto* dueLabel=new QLabel("Change: PKR 0.00",cartPanel); dueLabel->setObjectName("muted");
    auto* savePrint=new QPushButton("SAVE & PRINT",cartPanel); savePrint->setObjectName("primary"); savePrint->setMinimumHeight(32);
    auto* save=new QPushButton("SAVE",cartPanel); auto* hold=new QPushButton("HOLD",cartPanel); auto* resume=new QPushButton("RESUME",cartPanel); auto* clear=new QPushButton("CLEAR",cartPanel); clear->setObjectName("danger"); auto* cancel=new QPushButton("CANCEL",cartPanel);
    auto* primaryRow=new QHBoxLayout; primaryRow->addWidget(savePrint,3); primaryRow->addWidget(save,2); primaryRow->addWidget(hold,2);
    auto* secondaryRow=new QHBoxLayout; secondaryRow->addWidget(resume,2); secondaryRow->addWidget(clear,2); secondaryRow->addWidget(cancel,2);
    auto* feedback=new QLabel("",cartPanel); feedback->setObjectName("muted"); feedback->setWordWrap(true);
    cartLayout->addWidget(cartHeading); cartLayout->addWidget(cartHint); cartLayout->addWidget(cart,1); cartLayout->addLayout(rowActions); cartLayout->addWidget(discount); cartLayout->addWidget(summary); cartLayout->addWidget(received); cartLayout->addWidget(dueLabel); cartLayout->addLayout(primaryRow); cartLayout->addLayout(secondaryRow); cartLayout->addWidget(feedback);
    columns->addWidget(cartPanel,2); layout->addLayout(columns,1);
    const auto load=[this,products,search](){auto query=database_->prepare("SELECT id,name,sku,base_unit,stock_quantity,retail_price_paisa FROM products WHERE is_deleted=0 AND stock_quantity>0 AND (name LIKE ? OR COALESCE(sku,'') LIKE ? OR COALESCE(barcode,'') LIKE ?) ORDER BY name LIMIT 100");const auto term="%"+search->text().trimmed()+"%";query.bind(1,term);query.bind(2,term);query.bind(3,term);products->setRowCount(0);while(query.stepRow()){const int row=products->rowCount();products->insertRow(row);auto* item=new QTableWidgetItem(query.text(1));item->setData(Qt::UserRole,query.text(0));products->setItem(row,0,item);products->setItem(row,1,new QTableWidgetItem(query.text(2)));products->setItem(row,2,new QTableWidgetItem(QString::number(query.integer(4))));auto* price=new QTableWidgetItem(formatPaisa(query.integer(5)));price->setData(Qt::UserRole+2,query.integer(5));products->setItem(row,3,price);products->setItem(row,4,new QTableWidgetItem(query.text(3)));}};
    const auto computeTotal=[cart,discount,subtotalValue,discountValue,total](){qint64 subtotal{};for(int row=0;row<cart->rowCount();++row)subtotal+=cart->item(row,3)->data(Qt::UserRole).toLongLong();const qint64 disc=discount->value();const qint64 grand=subtotal-disc;subtotalValue->setText(QString("PKR %1").arg(formatPaisa(subtotal)));discountValue->setText(QString("PKR %1").arg(formatPaisa(disc)));total->setText(QString("PKR %1").arg(formatPaisa(grand)));return grand;};
    const auto refreshDue=[method,received,dueLabel](qint64 grand){if(method->currentData().toString()=="credit"){received->setEnabled(false);dueLabel->setText(QString("Balance due: PKR %1").arg(formatPaisa(grand)));}else{received->setEnabled(true);const auto paid=received->value();if(paid>=grand)dueLabel->setText(QString("Change: PKR %1").arg(formatPaisa(paid-grand)));else dueLabel->setText(QString("Balance due: PKR %1").arg(formatPaisa(grand-paid)));}};
    const auto refresh=[method,received,computeTotal,refreshDue](){const auto grand=computeTotal();if(method->currentData().toString()!="credit")received->setValue(grand);refreshDue(grand);};
    const auto addToCart=[products,cart,quantity,feedback,refresh]{const int row=products->currentRow();if(row<0)return;const auto id=products->item(row,0)->data(Qt::UserRole).toString();const auto count=quantity->value();const auto price=products->item(row,3)->data(Qt::UserRole+2).toLongLong();const auto stock=products->item(row,2)->text().toLongLong();for(int current=0;current<cart->rowCount();++current)if(cart->item(current,0)->data(Qt::UserRole).toString()==id){const auto newQuantity=cart->item(current,1)->text().toLongLong()+count;if(newQuantity>stock){feedback->setText(QString("Not enough stock — available: %1").arg(stock));return;}cart->item(current,1)->setText(QString::number(newQuantity));cart->item(current,3)->setText(formatPaisa(newQuantity*price));cart->item(current,3)->setData(Qt::UserRole,newQuantity*price);feedback->clear();refresh();return;}if(count>stock){feedback->setText(QString("Not enough stock — available: %1").arg(stock));return;}const int target=cart->rowCount();cart->insertRow(target);auto* name=new QTableWidgetItem(products->item(row,0)->text());name->setData(Qt::UserRole,id);name->setData(Qt::UserRole+1,products->item(row,4)->text());cart->setItem(target,0,name);cart->setItem(target,1,new QTableWidgetItem(QString::number(count)));auto* priceItem=new QTableWidgetItem(formatPaisa(price));priceItem->setData(Qt::UserRole+2,price);cart->setItem(target,2,priceItem);auto* amount=new QTableWidgetItem(formatPaisa(count*price));amount->setData(Qt::UserRole,count*price);cart->setItem(target,3,amount);feedback->clear();refresh();};
    load(); registerDataRefresh("inventory", load); connect(search,&QLineEdit::textChanged,this,[load]{load();}); connect(search,&QLineEdit::returnPressed,this,[products,addToCart]{if(products->rowCount()>0){products->setCurrentCell(0,0);addToCart();}}); connect(products,&QTableWidget::itemDoubleClicked,this,[addToCart]{addToCart();}); connect(add,&QPushButton::clicked,this,addToCart);
    connect(plus,&QPushButton::clicked,this,[this,cart,feedback,refresh]{const int row=cart->currentRow();if(row<0)return;const auto id=cart->item(row,0)->data(Qt::UserRole).toString();auto q=database_->prepare("SELECT stock_quantity FROM products WHERE id=? AND is_deleted=0");q.bind(1,id);if(!q.stepRow())return;const auto stock=q.integer(0);const auto newQuantity=cart->item(row,1)->text().toLongLong()+1;if(newQuantity>stock){feedback->setText(QString("Not enough stock — available: %1").arg(stock));return;}cart->item(row,1)->setText(QString::number(newQuantity));const auto price=cart->item(row,2)->data(Qt::UserRole+2).toLongLong();cart->item(row,3)->setText(formatPaisa(newQuantity*price));cart->item(row,3)->setData(Qt::UserRole,newQuantity*price);feedback->clear();refresh();});
    connect(minus,&QPushButton::clicked,this,[cart,refresh]{const int row=cart->currentRow();if(row<0)return;const auto current=cart->item(row,1)->text().toLongLong();if(current<=1){cart->removeRow(row);refresh();return;}const auto price=cart->item(row,2)->data(Qt::UserRole+2).toLongLong();cart->item(row,1)->setText(QString::number(current-1));cart->item(row,3)->setText(formatPaisa((current-1)*price));cart->item(row,3)->setData(Qt::UserRole,(current-1)*price);refresh();});
    connect(remove,&QPushButton::clicked,this,[cart,refresh]{const int row=cart->currentRow();if(row>=0){cart->removeRow(row);refresh();}});
    connect(discount,qOverload<int>(&QSpinBox::valueChanged),this,[received,computeTotal,refreshDue](int){const auto grand=computeTotal();received->setValue(grand);refreshDue(grand);});
    connect(method,qOverload<int>(&QComboBox::currentIndexChanged),this,[received,computeTotal,refreshDue](int){const auto grand=computeTotal();received->setValue(grand);refreshDue(grand);});
    connect(received,qOverload<int>(&QSpinBox::valueChanged),this,[computeTotal,refreshDue](int){refreshDue(computeTotal());});
    registerDataRefresh("customers", [this]{ reloadSalesCustomerCombo(); });
    const auto completeSale=[this,cart,discount,method,customer,received,load,refresh,feedback](bool printReceipt){if(!cart->rowCount()){feedback->setText("Add at least one product to the cart.");return;}try{pos::SaleRequest request;request.paymentMethod=method->currentData().toString();request.customerId=customer->currentData().toString();request.invoiceDiscount=discount->value();qint64 subtotal{};for(int row=0;row<cart->rowCount();++row){const auto count=cart->item(row,1)->text().toLongLong();const auto price=cart->item(row,2)->data(Qt::UserRole+2).toLongLong();subtotal+=count*price;request.lines.append({cart->item(row,0)->data(Qt::UserRole).toString(),{},count,price,0,cart->item(row,0)->data(Qt::UserRole+1).toString()});}if(request.invoiceDiscount>subtotal){QMessageBox::warning(this,"Invalid discount","Discount cannot exceed the cart subtotal.");return;}request.paidAmount=request.paymentMethod=="credit"?0:received->value();if(request.paidAmount>subtotal-request.invoiceDiscount){request.paidAmount=subtotal-request.invoiceDiscount;}if(request.paymentMethod=="mixed"){const auto maxTender=request.paidAmount;bool ok=false;const auto cashAmt=QInputDialog::getInt(this,"Mixed payment","Cash amount (paisa):",0,0,maxTender,1,&ok);if(!ok)return;const auto chequeAmt=QInputDialog::getInt(this,"Mixed payment","Cheque amount (paisa):",0,0,maxTender-cashAmt,1,&ok);if(!ok)return;const auto mobileAmt=maxTender-cashAmt-chequeAmt;if(cashAmt+chequeAmt+mobileAmt!=maxTender){QMessageBox::warning(this,"Invalid payment","Tender amounts must equal the sale total.");return;}if(cashAmt>0)request.tenders.append({"cash",cashAmt});if(chequeAmt>0)request.tenders.append({"cheque",chequeAmt});if(mobileAmt>0)request.tenders.append({"mobile_wallet",mobileAmt});}const auto due=subtotal-request.invoiceDiscount-request.paidAmount;if(due>0&&request.customerId.isEmpty()){QMessageBox::warning(this,"Customer required","An unpaid balance needs a customer. Select one or increase the amount received.");return;}QList<pos::ThermalReceiptItem> receiptItems;for(int row=0;row<cart->rowCount();++row)receiptItems.append({cart->item(row,0)->text(),cart->item(row,1)->text().toLongLong(),cart->item(row,3)->data(Qt::UserRole).toLongLong()});const auto sale=pos_->completeSale(request);cart->setRowCount(0);discount->setValue(0);received->setValue(0);method->setCurrentIndex(0);customer->setCurrentIndex(0);refresh();load();if(printReceipt){try{const auto path=pos::SettingsService(database_).value("printer.thermal_path");if(path.trimmed().isEmpty()){feedback->setText(QString("Invoice %1 saved. No printer is configured, so no receipt was printed.").arg(sale.invoiceNo));}else{pos::ThermalPrintService::writeRaw(path,pos::ThermalPrintService::receiptBytes(pos::SettingsService(database_).value("business.name","Nexora POS"),sale.invoiceNo,receiptItems,sale.total));feedback->setText(QString("Invoice %1 saved and receipt printed.").arg(sale.invoiceNo));}}catch(const std::exception& e){feedback->setText(QString("Invoice %1 saved but the receipt could not be printed: %2").arg(sale.invoiceNo,e.what()));}}else feedback->setText(QString("Invoice %1 saved.").arg(sale.invoiceNo));}catch(const std::exception& error){QMessageBox::critical(this,"Sale failed",error.what());}};
    connect(save,&QPushButton::clicked,this,[completeSale]{completeSale(false);}); connect(savePrint,&QPushButton::clicked,this,[completeSale]{completeSale(true);});
    connect(hold,&QPushButton::clicked,this,[this,cart,discount,refresh,feedback]{if(!cart->rowCount())return;try{QList<pos::SuspendedLine> lines;for(int row=0;row<cart->rowCount();++row)lines.append({cart->item(row,0)->data(Qt::UserRole).toString(),cart->item(row,1)->text().toLongLong(),cart->item(row,2)->data(Qt::UserRole+2).toLongLong(),cart->item(row,0)->data(Qt::UserRole+1).toString()});pos::SuspendedSaleService(database_).save(lines);cart->setRowCount(0);discount->setValue(0);refresh();feedback->setText("Sale held and saved locally. Use Resume to bring it back.");}catch(const std::exception& error){QMessageBox::critical(this,"Could not hold sale",error.what());}});
    connect(resume,&QPushButton::clicked,this,[this,cart,discount,refresh,feedback]{try{const auto saved=pos::SuspendedSaleService(database_).list();if(saved.isEmpty()){feedback->setText("No held sales are available.");return;}QStringList choices;for(const auto& sale:saved)choices.append(sale.createdAt+" ("+sale.id.left(8)+")");bool ok=false;const auto selected=QInputDialog::getItem(this,"Resume sale","Held cart:",choices,0,false,&ok);if(!ok)return;const auto sale=pos::SuspendedSaleService(database_).load(saved.at(choices.indexOf(selected)).id);cart->setRowCount(0);for(const auto& line:sale.lines){auto product=database_->prepare("SELECT name,stock_quantity FROM products WHERE id=? AND is_deleted=0");product.bind(1,line.productId);if(!product.stepRow()||product.integer(1)<line.quantity)throw pos::DatabaseError("a held product is unavailable or out of stock");const int row=cart->rowCount();cart->insertRow(row);auto* name=new QTableWidgetItem(product.text(0));name->setData(Qt::UserRole,line.productId);name->setData(Qt::UserRole+1,line.unit);cart->setItem(row,0,name);cart->setItem(row,1,new QTableWidgetItem(QString::number(line.quantity)));auto* price=new QTableWidgetItem(formatPaisa(line.unitPrice));price->setData(Qt::UserRole+2,line.unitPrice);cart->setItem(row,2,price);auto* amount=new QTableWidgetItem(formatPaisa(line.quantity*line.unitPrice));amount->setData(Qt::UserRole,line.quantity*line.unitPrice);cart->setItem(row,3,amount);}pos::SuspendedSaleService(database_).remove(sale.id);refresh();feedback->setText("Held sale restored to the cart.");}catch(const std::exception& error){QMessageBox::critical(this,"Could not resume sale",error.what());}});
    connect(clear,&QPushButton::clicked,this,[this,cart,discount,refresh,feedback]{if(!cart->rowCount())return;if(QMessageBox::question(this,"Clear sale","Clear the current cart?")==QMessageBox::Yes){cart->setRowCount(0);discount->setValue(0);refresh();feedback->setText("Cart cleared.");}});
    connect(cancel,&QPushButton::clicked,this,[this,cart,discount,refresh]{if(cart->rowCount()&&QMessageBox::question(this,"Cancel sale","Abandon this sale and return to the dashboard?")!=QMessageBox::Yes)return;cart->setRowCount(0);discount->setValue(0);refresh();goToPage("Dashboard");});
    refresh(); return page;
}
QWidget* MainWindow::makePurchases(){
    auto* page=new QWidget;auto* layout=new QVBoxLayout(page);layout->setContentsMargins(0,0,0,0);
    auto* title=new QLabel("Purchases",page);title->setObjectName("pageTitle");
    auto* refreshLists=new QPushButton("Refresh lists",page);
    auto* topRow=new QHBoxLayout; topRow->addWidget(title); topRow->addStretch(); topRow->addWidget(refreshLists);
    layout->addLayout(topRow);
    auto* supplier=new QComboBox(page);supplier->setObjectName("purchaseSupplier");supplier->setPlaceholderText("Select supplier");auto query=database_->prepare("SELECT id,name FROM suppliers WHERE is_archived=0 ORDER BY name");while(query.stepRow())supplier->addItem(query.text(1),query.text(0));layout->addWidget(supplier);
    auto* controls=new QHBoxLayout;auto* product=new QComboBox(page);product->setObjectName("purchaseProduct");product->setPlaceholderText("Select product");auto products=database_->prepare("SELECT id,name,base_unit FROM products WHERE is_deleted=0 ORDER BY name");while(products.stepRow())product->addItem(products.text(1)+" ("+products.text(2)+")",products.text(0));auto* quantity=new QSpinBox(page);quantity->setRange(1,1000000);auto* price=new QSpinBox(page);price->setRange(0,1000000000);price->setPrefix("Cost paisa: ");auto* batch=new QLineEdit(page);batch->setPlaceholderText("Batch (if tracked)");auto* expiry=new QDateEdit(QDate::currentDate().addYears(1),page);expiry->setCalendarPopup(true);auto* add=new QPushButton("Add item",page);controls->addWidget(product,2);controls->addWidget(quantity);controls->addWidget(price);controls->addWidget(batch);controls->addWidget(expiry);controls->addWidget(add);layout->addLayout(controls);
    auto* cart=new QTableWidget(page);cart->setColumnCount(5);cart->setHorizontalHeaderLabels({"Product","Quantity","Unit cost","Batch","Expiry"});cart->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);cart->setSelectionBehavior(QAbstractItemView::SelectRows);cart->setSelectionMode(QAbstractItemView::SingleSelection);cart->setEditTriggers(QAbstractItemView::NoEditTriggers);layout->addWidget(cart,1);
    auto* cartActions=new QHBoxLayout; auto* remove=new QPushButton("Remove item",page); remove->setObjectName("danger"); auto* clear=new QPushButton("Clear cart",page); cartActions->addWidget(remove); cartActions->addWidget(clear); cartActions->addStretch();
    layout->addLayout(cartActions);
    auto* save=new QPushButton("Receive purchase on credit",page);save->setObjectName("primary");layout->addWidget(save);
    connect(add,&QPushButton::clicked,this,[this,product,quantity,price,batch,expiry,cart]{if(product->currentIndex()<0){QMessageBox::information(this,"No product selected","Please select a product from the list. If you haven't created any products yet, go to the 'Inventory' tab to add them first.");return;}const auto id=product->currentData().toString();auto p=database_->prepare("SELECT base_unit,track_batches FROM products WHERE id=?");p.bind(1,id);if(!p.stepRow())return;const bool tracked=p.integer(1)!=0;if(tracked&&batch->text().trimmed().isEmpty()){QMessageBox::information(this,"Batch required","This product tracks batches. Enter a batch number.");return;}const int row=cart->rowCount();cart->insertRow(row);auto* name=new QTableWidgetItem(product->currentText());name->setData(Qt::UserRole,id);name->setData(Qt::UserRole+1,p.text(0));cart->setItem(row,0,name);cart->setItem(row,1,new QTableWidgetItem(QString::number(quantity->value())));cart->setItem(row,2,new QTableWidgetItem(QString::number(price->value())));cart->setItem(row,3,new QTableWidgetItem(batch->text().trimmed()));cart->setItem(row,4,new QTableWidgetItem(expiry->date().toString(Qt::ISODate)));batch->clear();});
    connect(remove,&QPushButton::clicked,this,[cart]{const int row=cart->currentRow(); if(row>=0) cart->removeRow(row);});
    connect(clear,&QPushButton::clicked,this,[cart]{cart->setRowCount(0);});
    connect(refreshLists,&QPushButton::clicked,this,[this]{ reloadPurchaseCombos(); });
    registerDataRefresh("inventory", [this]{ reloadPurchaseCombos(); });
    registerDataRefresh("suppliers", [this]{ reloadPurchaseCombos(); });
    registerDataRefresh("purchases", [this]{ reloadPurchaseCombos(); });
    connect(save,&QPushButton::clicked,this,[this,supplier,cart]{if(supplier->currentIndex()<0||cart->rowCount()==0){QMessageBox::information(this,"Purchase required","Choose a supplier and add at least one item.");return;}try{pos::PurchaseRequest request;request.supplierId=supplier->currentData().toString();for(int row=0;row<cart->rowCount();++row){const auto* name=cart->item(row,0);request.lines.append({name->data(Qt::UserRole).toString(),cart->item(row,1)->text().toLongLong(),cart->item(row,2)->text().toLongLong(),0,0,name->data(Qt::UserRole+1).toString(),cart->item(row,3)->text(),QDate::fromString(cart->item(row,4)->text(),Qt::ISODate)});}const auto result=pos::PurchaseService(database_).completePurchase(request);cart->setRowCount(0);QMessageBox::information(this,"Purchase received",QString("Purchase %1 saved.").arg(result.invoiceNo));}catch(const std::exception& error){QMessageBox::critical(this,"Purchase failed",error.what());}});return page;
}
QWidget* MainWindow::makeCustomers(){auto* page=new QWidget;auto* layout=new QVBoxLayout(page);layout->setContentsMargins(0,0,0,0);auto* title=new QLabel("Customers",page);title->setObjectName("pageTitle");layout->addWidget(title);auto* actions=new QHBoxLayout;auto* add=new QPushButton("Add customer",page);add->setObjectName("primary");auto* payment=new QPushButton("Record payment",page);auto* refresh=new QPushButton("Refresh",page);actions->addWidget(add);actions->addWidget(payment);actions->addWidget(refresh);actions->addStretch();layout->addLayout(actions);auto* table=new QTableWidget(page);table->setColumnCount(5);table->setHorizontalHeaderLabels({"Customer","Phone","Credit limit","Outstanding","Terms (days)"});table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);table->setSelectionBehavior(QAbstractItemView::SelectRows);table->setEditTriggers(QAbstractItemView::NoEditTriggers);layout->addWidget(table,1);const auto load=[this,table](){auto q=database_->prepare("SELECT id,name,phone,credit_limit_paisa,balance_paisa,payment_terms_days FROM customers WHERE is_deleted=0 ORDER BY name");table->setRowCount(0);while(q.stepRow()){const int row=table->rowCount();table->insertRow(row);table->setItem(row,0,new QTableWidgetItem(q.text(1)));table->item(row,0)->setData(Qt::UserRole,q.text(0));table->setItem(row,1,new QTableWidgetItem(q.text(2)));table->setItem(row,2,new QTableWidgetItem(QString("PKR %1").arg(formatPaisa(q.integer(3)))));auto* outstanding=new QTableWidgetItem(QString("PKR %1").arg(formatPaisa(q.integer(4))));outstanding->setForeground(q.integer(4)>0?QColor("#B3261E"):QColor("#16A34A"));table->setItem(row,3,outstanding);table->setItem(row,4,new QTableWidgetItem(QString::number(q.integer(5))));}};load();registerDataRefresh("customers",load);registerDataRefresh("sales",load);connect(refresh,&QPushButton::clicked,this,[load]{load();});connect(add,&QPushButton::clicked,this,[this,load]{bool ok=false;const auto name=QInputDialog::getText(this,"New customer","Customer name:",QLineEdit::Normal,{},&ok);if(!ok||name.trimmed().isEmpty())return;const auto phone=QInputDialog::getText(this,"New customer","Phone:",QLineEdit::Normal,{},&ok);if(!ok)return;const auto limit=QInputDialog::getInt(this,"New customer","Credit limit (paisa):",0,0,1000000000,1,&ok);if(!ok)return;try{pos::CustomerService(database_).create({{},name,phone,limit,0,false});load();}catch(const std::exception& error){QMessageBox::critical(this,"Could not add customer",error.what());}});connect(payment,&QPushButton::clicked,this,[this,table,load]{const auto row=table->currentRow();if(row<0){QMessageBox::information(this,"Record payment","Select a customer first.");return;}const auto customerId=table->item(row,0)->data(Qt::UserRole).toString();auto invoices=database_->prepare("SELECT id,invoice_no,due_paisa FROM sales WHERE customer_id=? AND due_paisa>0 AND status!='voided' ORDER BY created_at");invoices.bind(1,customerId);QStringList choices;QList<QString> ids;while(invoices.stepRow()){ids.append(invoices.text(0));choices.append(invoices.text(1)+" (due "+QString::number(invoices.integer(2))+" paisa)");}if(choices.isEmpty()){QMessageBox::information(this,"Record payment","This customer has no outstanding invoices.");return;}bool ok=false;const auto selected=QInputDialog::getItem(this,"Allocate payment","Invoice:",choices,0,false,&ok);if(!ok)return;const auto amount=QInputDialog::getInt(this,"Allocate payment","Amount (paisa):",0,1,1000000000,1,&ok);if(!ok)return;const auto method=QInputDialog::getItem(this,"Allocate payment","Method:",{"cash","cheque","mobile_wallet","bank"},0,false,&ok);if(!ok)return;const auto index=choices.indexOf(selected);try{pos::PaymentService(database_).recordCustomerPayment(customerId,{{ids.at(index),amount}},method,"Customer payment");load();}catch(const std::exception& error){QMessageBox::critical(this,"Could not record payment",error.what());}});return page;}
QWidget* MainWindow::makeSuppliers(){
    auto* page=new QWidget; auto* layout=new QVBoxLayout(page); layout->setContentsMargins(0,0,0,0);
    auto* title=new QLabel("Suppliers",page); title->setObjectName("pageTitle"); layout->addWidget(title);
    auto* actions=new QHBoxLayout; auto* add=new QPushButton("Add supplier",page); add->setObjectName("primary");
    auto* archive=new QPushButton("Archive selected",page); archive->setObjectName("danger"); auto* ledger=new QPushButton("View ledger",page); auto* refresh=new QPushButton("Refresh",page);
    actions->addWidget(add); actions->addWidget(archive); actions->addWidget(ledger); actions->addWidget(refresh); actions->addStretch(); layout->addLayout(actions);
    auto* table=new QTableWidget(page); table->setColumnCount(5); table->setHorizontalHeaderLabels({"Supplier","Contact","Phone","Address","Payable"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); table->setSelectionBehavior(QAbstractItemView::SelectRows); table->setEditTriggers(QAbstractItemView::NoEditTriggers); layout->addWidget(table,1);
    auto* ledgerTitle=new QLabel("Supplier ledger",page); ledgerTitle->setObjectName("sectionTitle"); layout->addWidget(ledgerTitle);
    auto* ledgerTable=new QTableWidget(page); ledgerTable->setColumnCount(5); ledgerTable->setHorizontalHeaderLabels({"Date","Entry","Reference","Debit","Credit"});
    ledgerTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); ledgerTable->setEditTriggers(QAbstractItemView::NoEditTriggers); ledgerTable->setFixedHeight(170); layout->addWidget(ledgerTable);
    const auto load=[this,table](){auto q=database_->prepare("SELECT id,name,contact_person,phone,address,balance_paisa FROM suppliers WHERE is_archived=0 ORDER BY name");table->setRowCount(0);while(q.stepRow()){const int row=table->rowCount();table->insertRow(row);table->setItem(row,0,new QTableWidgetItem(q.text(1)));table->item(row,0)->setData(Qt::UserRole,q.text(0));table->setItem(row,1,new QTableWidgetItem(q.text(2)));table->setItem(row,2,new QTableWidgetItem(q.text(3)));table->setItem(row,3,new QTableWidgetItem(q.text(4)));table->setItem(row,4,new QTableWidgetItem(QString("PKR %1").arg(formatPaisa(q.integer(5)))));}};
    const auto loadLedger=[this,table,ledgerTable](){const auto row=table->currentRow(); ledgerTable->setRowCount(0); if(row<0)return; try{const auto entries=pos::SupplierService(database_).ledger(table->item(row,0)->data(Qt::UserRole).toString()); for(const auto& entry:entries){const int target=ledgerTable->rowCount();ledgerTable->insertRow(target);ledgerTable->setItem(target,0,new QTableWidgetItem(entry.createdAt));ledgerTable->setItem(target,1,new QTableWidgetItem(entry.entryType));ledgerTable->setItem(target,2,new QTableWidgetItem(entry.referenceId));ledgerTable->setItem(target,3,new QTableWidgetItem(QString("PKR %1").arg(formatPaisa(entry.debit))));ledgerTable->setItem(target,4,new QTableWidgetItem(QString("PKR %1").arg(formatPaisa(entry.credit))));}}catch(const std::exception& error){QMessageBox::critical(this,"Could not load supplier ledger",error.what());}};
    load(); registerDataRefresh("suppliers", load); registerDataRefresh("purchases", load); connect(refresh,&QPushButton::clicked,this,[load,loadLedger]{load();loadLedger();}); connect(table,&QTableWidget::itemSelectionChanged,this,loadLedger); connect(ledger,&QPushButton::clicked,this,loadLedger);
    connect(add,&QPushButton::clicked,this,[this,load]{bool ok=false;const auto name=QInputDialog::getText(this,"New supplier","Supplier name:",QLineEdit::Normal,{},&ok);if(!ok||name.trimmed().isEmpty())return;const auto contact=QInputDialog::getText(this,"New supplier","Contact person:",QLineEdit::Normal,{},&ok);if(!ok)return;const auto phone=QInputDialog::getText(this,"New supplier","Phone:",QLineEdit::Normal,{},&ok);if(!ok)return;const auto address=QInputDialog::getText(this,"New supplier","Address:",QLineEdit::Normal,{},&ok);if(!ok)return;const auto opening=QInputDialog::getInt(this,"New supplier","Opening payable (paisa):",0,0,1000000000,1,&ok);if(!ok)return;try{pos::SupplierService(database_).create({{},name,contact,phone,address,opening,false});load();}catch(const std::exception& error){QMessageBox::critical(this,"Could not add supplier",error.what());}});
    connect(archive,&QPushButton::clicked,this,[this,table,load,loadLedger]{const auto row=table->currentRow();if(row<0){QMessageBox::information(this,"Archive supplier","Select a supplier first.");return;}const auto id=table->item(row,0)->data(Qt::UserRole).toString();if(QMessageBox::question(this,"Archive supplier","Archive the selected supplier?")==QMessageBox::Yes){if(!authorizeSensitiveAction("archive a supplier"))return;try{pos::SupplierService(database_).archive(id);load();loadLedger();}catch(const std::exception& error){QMessageBox::critical(this,"Could not archive supplier",error.what());}}}); return page;
}
QWidget* MainWindow::makeCashManagement(){auto* page=new QWidget;auto* layout=new QVBoxLayout(page);layout->setContentsMargins(0,0,0,0);auto* title=new QLabel("Cash Management",page);title->setObjectName("pageTitle");layout->addWidget(title);auto* status=new QLabel(page);status->setObjectName("sectionTitle");auto* open=new QPushButton("Open shift",page);open->setObjectName("primary");auto* close=new QPushButton("Close shift",page);close->setObjectName("danger");auto* actions=new QHBoxLayout;actions->addWidget(open);actions->addWidget(close);actions->addStretch();layout->addLayout(actions);layout->addWidget(status);layout->addStretch();const auto refresh=[this,status]{const auto id=pos::activeShiftId(*database_);status->setText(id.isEmpty()?"Till status: CLOSED":"Till status: OPEN\nShift: "+id);};refresh();registerDataRefresh("cash",refresh);connect(open,&QPushButton::clicked,this,[this,refresh]{bool ok=false;const auto opening=QInputDialog::getInt(this,"Open shift","Opening cash (paisa):",0,0,1000000000,1,&ok);if(!ok)return;try{pos::ShiftService(database_).open(opening);refresh();}catch(const std::exception& error){QMessageBox::critical(this,"Could not open shift",error.what());}});connect(close,&QPushButton::clicked,this,[this,refresh]{const auto id=pos::activeShiftId(*database_);if(id.isEmpty()){QMessageBox::information(this,"No open shift","Open a shift before closing the till.");return;}if(!authorizeSensitiveAction("close the shift"))return;bool ok=false;const auto counted=QInputDialog::getInt(this,"Close shift","Counted cash (paisa):",0,0,1000000000,1,&ok);if(!ok)return;try{const auto result=pos::ShiftService(database_).close(id,counted);refresh();QMessageBox::information(this,"Shift closed",QString("Expected: %1 paisa\nDifference: %2 paisa").arg(result.expected).arg(result.difference));}catch(const std::exception& error){QMessageBox::critical(this,"Could not close shift",error.what());}});return page;}
QWidget* MainWindow::makeCheques(){auto* page=new QWidget;auto* layout=new QVBoxLayout(page);layout->setContentsMargins(0,0,0,0);auto* title=new QLabel("Cheques",page);title->setObjectName("pageTitle");layout->addWidget(title);auto* actions=new QHBoxLayout;auto* add=new QPushButton("Record cheque",page);add->setObjectName("primary");auto* status=new QPushButton("Update status",page);auto* refresh=new QPushButton("Refresh",page);actions->addWidget(add);actions->addWidget(status);actions->addWidget(refresh);actions->addStretch();layout->addLayout(actions);auto* table=new QTableWidget(page);table->setColumnCount(6);table->setHorizontalHeaderLabels({"Direction","Cheque no.","Bank","Amount","Due date","Status"});table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);table->setSelectionBehavior(QAbstractItemView::SelectRows);table->setEditTriggers(QAbstractItemView::NoEditTriggers);layout->addWidget(table,1);const auto load=[this,table](){auto q=database_->prepare("SELECT id,direction,cheque_no,bank,amount_paisa,due_date,status FROM cheques ORDER BY due_date,cheque_no");table->setRowCount(0);while(q.stepRow()){const int row=table->rowCount();table->insertRow(row);table->setItem(row,0,new QTableWidgetItem(q.text(1)));table->setItem(row,1,new QTableWidgetItem(q.text(2)));table->item(row,1)->setData(Qt::UserRole,q.text(0));table->setItem(row,2,new QTableWidgetItem(q.text(3)));table->setItem(row,3,new QTableWidgetItem(QString("PKR %1").arg(formatPaisa(q.integer(4)))));table->setItem(row,4,new QTableWidgetItem(q.text(5)));table->setItem(row,5,new QTableWidgetItem(q.text(6)));}};load();connect(refresh,&QPushButton::clicked,this,[load]{load();});connect(add,&QPushButton::clicked,this,[this,load]{bool ok=false;const auto direction=QInputDialog::getItem(this,"Record cheque","Direction:",{"received","issued"},0,false,&ok);if(!ok)return;const auto number=QInputDialog::getText(this,"Record cheque","Cheque number:",QLineEdit::Normal,{},&ok);if(!ok||number.trimmed().isEmpty())return;const auto bank=QInputDialog::getText(this,"Record cheque","Bank:",QLineEdit::Normal,{},&ok);if(!ok)return;const auto amount=QInputDialog::getInt(this,"Record cheque","Amount (paisa):",0,1,1000000000,1,&ok);if(!ok)return;const auto due=QInputDialog::getText(this,"Record cheque","Due date (YYYY-MM-DD):",QLineEdit::Normal,QDate::currentDate().toString(Qt::ISODate),&ok);if(!ok)return;const auto date=QDate::fromString(due,Qt::ISODate);if(!date.isValid()){QMessageBox::warning(this,"Invalid date","Enter a valid date in YYYY-MM-DD format.");return;}try{pos::ChequeService(database_).record({{},direction,{},number,bank,{},amount,date});load();}catch(const std::exception& error){QMessageBox::critical(this,"Could not record cheque",error.what());}});connect(status,&QPushButton::clicked,this,[this,table,load]{const auto row=table->currentRow();if(row<0){QMessageBox::information(this,"Update cheque","Select a cheque first.");return;}const auto id=table->item(row,1)->data(Qt::UserRole).toString();bool ok=false;const auto next=QInputDialog::getItem(this,"Update cheque status","Status:",{"pending","deposited","cleared","bounced"},0,false,&ok);if(!ok)return;try{pos::ChequeService(database_).setStatus(id,next);load();}catch(const std::exception& error){QMessageBox::critical(this,"Could not update cheque",error.what());}});return page;}
QWidget* MainWindow::makeReports(){
    auto* page=new QWidget; auto* layout=new QVBoxLayout(page); layout->setContentsMargins(0,0,0,0);
    auto* title=new QLabel("Reports",page); title->setObjectName("pageTitle"); layout->addWidget(title);
    auto* filters=new QHBoxLayout; auto* from=new QDateEdit(QDate::currentDate().addDays(-30),page); auto* to=new QDateEdit(QDate::currentDate(),page); from->setCalendarPopup(true); to->setCalendarPopup(true);
    auto* refresh=new QPushButton("Run report",page); refresh->setObjectName("primary"); auto* exportCsv=new QPushButton("Export CSV",page); auto* exportPdfButton=new QPushButton("Export PDF",page); auto* exportExcelButton=new QPushButton("Export Excel",page); auto* printButton=new QPushButton("Print A4",page);
    filters->addWidget(new QLabel("From",page)); filters->addWidget(from); filters->addWidget(new QLabel("To",page)); filters->addWidget(to); filters->addWidget(refresh); filters->addWidget(exportCsv); filters->addWidget(exportPdfButton); filters->addWidget(exportExcelButton); filters->addWidget(printButton); filters->addStretch(); layout->addLayout(filters);
    auto* summary=new QTableWidget(page); summary->setColumnCount(2); summary->setRowCount(6); summary->setHorizontalHeaderLabels({"Metric","Value"}); summary->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); summary->setEditTriggers(QAbstractItemView::NoEditTriggers);
    const QStringList labels={"Sales","Purchases","Receivables","Payables","Inventory value","Low-stock products"}; for(int row=0;row<labels.size();++row)summary->setItem(row,0,new QTableWidgetItem(labels[row])); layout->addWidget(summary,1);
    const auto load=[this,from,to,summary]{try{const auto r=pos::ReportService(database_).summary(from->date(),to->date());const QList<qint64> values={r.sales,r.purchases,r.receivables,r.payables,r.inventoryValue,r.lowStock};for(int row=0;row<values.size();++row)summary->setItem(row,1,new QTableWidgetItem(QString::number(values[row])));}catch(const std::exception& error){QMessageBox::critical(this,"Could not run report",error.what());}};
    const auto exportReport=[this,from,to,labels]{const auto fileName=QFileDialog::getSaveFileName(this,"Export report",{},"CSV files (*.csv)");if(fileName.isEmpty())return;try{const auto r=pos::ReportService(database_).summary(from->date(),to->date());const QList<qint64> values={r.sales,r.purchases,r.receivables,r.payables,r.inventoryValue,r.lowStock};QFile file(fileName);if(!file.open(QIODevice::WriteOnly|QIODevice::Text))throw pos::DatabaseError("could not open export file");QTextStream stream(&file);stream<<"Metric,Value\n";for(int row=0;row<labels.size();++row)stream<<labels[row]<<","<<values[row]<<"\n";file.close();QMessageBox::information(this,"Report exported","The report was exported to CSV.");}catch(const std::exception& error){QMessageBox::critical(this,"Could not export report",error.what());}};
    const auto exportPdfFile=[this,from,to,labels]{const auto fileName=QFileDialog::getSaveFileName(this,"Export report",{},"PDF files (*.pdf)");if(fileName.isEmpty())return;try{const auto r=pos::ReportService(database_).summary(from->date(),to->date());const QList<qint64> values={r.sales,r.purchases,r.receivables,r.payables,r.inventoryValue,r.lowStock};QPrinter printer(QPrinter::HighResolution);printer.setOutputFormat(QPrinter::PdfFormat);printer.setOutputFileName(fileName);QPainter painter(&printer);painter.setFont(QFont("Arial",14));painter.drawText(100,100,"Nexora POS business report");painter.setFont(QFont("Arial",10));painter.drawText(100,130,QString("Period: %1 to %2").arg(from->date().toString(Qt::ISODate),to->date().toString(Qt::ISODate)));int y=180;for(int row=0;row<labels.size();++row){painter.drawText(100,y,QString("%1: %2").arg(labels[row]).arg(values[row]));y+=28;}painter.end();QMessageBox::information(this,"Report exported","The report was exported to PDF.");}catch(const std::exception& error){QMessageBox::critical(this,"Could not export report",error.what());}};
    const auto exportExcelFile=[this,from,to,labels]{const auto fileName=QFileDialog::getSaveFileName(this,"Export report",{},"Excel workbooks (*.xlsx)");if(fileName.isEmpty())return;try{const auto r=pos::ReportService(database_).summary(from->date(),to->date());const QList<qint64> values={r.sales,r.purchases,r.receivables,r.payables,r.inventoryValue,r.lowStock};QList<QStringList> rows;for(int row=0;row<labels.size();++row)rows.append({labels[row],QString::number(values[row])});pos::ExcelExportService::writeWorkbook(fileName,{"Metric","Value"},rows);QMessageBox::information(this,"Report exported","The report was exported to Excel.");}catch(const std::exception& error){QMessageBox::critical(this,"Could not export report",error.what());}};
    const auto printReport=[this,from,to,labels]{const auto r=pos::ReportService(database_).summary(from->date(),to->date());QPrinter printer(QPrinter::HighResolution);QPrintDialog dialog(&printer,this);if(dialog.exec()!=QDialog::Accepted)return;QPainter painter(&printer);painter.setFont(QFont("Arial",14));painter.drawText(100,100,"Nexora POS business report");painter.setFont(QFont("Arial",10));painter.drawText(100,130,QString("Period: %1 to %2").arg(from->date().toString(Qt::ISODate),to->date().toString(Qt::ISODate)));const QList<qint64> values={r.sales,r.purchases,r.receivables,r.payables,r.inventoryValue,r.lowStock};int y=180;for(int row=0;row<labels.size();++row){painter.drawText(100,y,QString("%1: %2").arg(labels[row]).arg(values[row]));y+=28;}painter.end();};
    load(); connect(refresh,&QPushButton::clicked,this,load); connect(exportCsv,&QPushButton::clicked,this,exportReport); connect(exportPdfButton,&QPushButton::clicked,this,exportPdfFile); connect(exportExcelButton,&QPushButton::clicked,this,exportExcelFile); connect(printButton,&QPushButton::clicked,this,printReport); return page;
}
QWidget* MainWindow::makeAuditLog(){auto* page=new QWidget;auto* layout=new QVBoxLayout(page);layout->setContentsMargins(0,0,0,0);auto* title=new QLabel("Audit Log",page);title->setObjectName("pageTitle");layout->addWidget(title);auto* actions=new QHBoxLayout;auto* filter=new QLineEdit(page);filter->setPlaceholderText("Filter by action (optional)");auto* refresh=new QPushButton("Refresh",page);actions->addWidget(filter,1);actions->addWidget(refresh);layout->addLayout(actions);auto* table=new QTableWidget(page);table->setColumnCount(5);table->setHorizontalHeaderLabels({"Action","Entity","Entity ID","Detail","Created"});table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);table->setEditTriggers(QAbstractItemView::NoEditTriggers);layout->addWidget(table,1);const auto load=[this,filter,table]{try{const auto rows=pos::AuditService(database_).recent(filter->text().trimmed());table->setRowCount(0);for(const auto& entry:rows){const int row=table->rowCount();table->insertRow(row);table->setItem(row,0,new QTableWidgetItem(entry.action));table->setItem(row,1,new QTableWidgetItem(entry.entityType));table->setItem(row,2,new QTableWidgetItem(entry.entityId));table->setItem(row,3,new QTableWidgetItem(entry.detail));table->setItem(row,4,new QTableWidgetItem(entry.createdAt));}}catch(const std::exception& error){QMessageBox::critical(this,"Could not load audit log",error.what());}};load();connect(refresh,&QPushButton::clicked,this,load);return page;}
QWidget* MainWindow::makeSettings(){
    auto* page=new QWidget;auto* layout=new QVBoxLayout(page);layout->setContentsMargins(0,0,0,0);layout->setSpacing(16);
    auto* title=new QLabel("Settings",page);title->setObjectName("pageTitle");layout->addWidget(title);
    auto* identityCard=new QFrame(page);identityCard->setObjectName("panel");auto* identityLayout=new QVBoxLayout(identityCard);identityLayout->setContentsMargins(20,18,20,18);identityLayout->setSpacing(14);
    auto* identityTitle=new QLabel("Business identity & defaults",identityCard);identityTitle->setObjectName("sectionTitle");identityLayout->addWidget(identityTitle);
    auto* business=new QLineEdit(identityCard);auto* phone=new QLineEdit(identityCard);auto* currency=new QLineEdit(identityCard);auto* footer=new QLineEdit(identityCard);auto* backupHours=new QSpinBox(identityCard);backupHours->setRange(0,168);backupHours->setSuffix(" hours (0 disables)");auto* thermalPath=new QLineEdit(identityCard);thermalPath->setPlaceholderText("Raw printer path or shared printer device");
    auto* form=new QGridLayout;form->setHorizontalSpacing(14);form->setVerticalSpacing(12);form->setColumnStretch(1,1);
    form->addWidget(new QLabel("Business name",identityCard),0,0);form->addWidget(business,0,1);form->addWidget(new QLabel("Phone",identityCard),1,0);form->addWidget(phone,1,1);form->addWidget(new QLabel("Currency",identityCard),2,0);form->addWidget(currency,2,1);form->addWidget(new QLabel("Receipt footer",identityCard),3,0);form->addWidget(footer,3,1);form->addWidget(new QLabel("Automatic backup interval",identityCard),4,0);form->addWidget(backupHours,4,1);form->addWidget(new QLabel("Thermal printer path",identityCard),5,0);form->addWidget(thermalPath,5,1);identityLayout->addLayout(form);
    auto* save=new QPushButton("Save settings",identityCard);save->setObjectName("primary");auto* saveRow=new QHBoxLayout;saveRow->addWidget(save);saveRow->addStretch();identityLayout->addLayout(saveRow);layout->addWidget(identityCard);
    auto* securityCard=new QFrame(page);securityCard->setObjectName("panel");auto* securityLayout=new QVBoxLayout(securityCard);securityLayout->setContentsMargins(20,18,20,18);securityLayout->setSpacing(12);
    auto* securityTitle=new QLabel("Security, printing & notifications",securityCard);securityTitle->setObjectName("sectionTitle");securityLayout->addWidget(securityTitle);
    auto* setPin=new QPushButton("Set or change security PIN",securityCard);auto* clearPin=new QPushButton("Clear security PIN",securityCard);clearPin->setObjectName("danger");auto* notifications=new QPushButton("View notifications",securityCard);auto* testReceipt=new QPushButton("Print test receipt",securityCard);auto* testLabel=new QPushButton("Print test barcode label",securityCard);auto* pinStatus=new QLabel(securityCard);pinStatus->setObjectName("muted");
    securityLayout->addWidget(setPin);securityLayout->addWidget(clearPin);securityLayout->addWidget(notifications);securityLayout->addWidget(testReceipt);securityLayout->addWidget(testLabel);securityLayout->addWidget(pinStatus);layout->addWidget(securityCard);layout->addStretch();
    const auto load=[this,business,phone,currency,footer,backupHours,thermalPath,pinStatus]{pos::SettingsService service(database_);business->setText(service.value("business.name"));phone->setText(service.value("business.phone"));currency->setText(service.value("business.currency","PKR"));footer->setText(service.value("receipt.footer"));backupHours->setValue(service.value("backup.interval_hours","0").toInt());thermalPath->setText(service.value("printer.thermal_path"));pinStatus->setText(pos::SecurityService(database_).hasPin()?"Sensitive-action PIN: configured":"Sensitive-action PIN: not configured");};load();
    connect(save,&QPushButton::clicked,this,[this,business,phone,currency,footer,backupHours,thermalPath]{try{pos::SettingsService service(database_);service.setValue("business.name",business->text().trimmed());service.setValue("business.phone",phone->text().trimmed());service.setValue("business.currency",currency->text().trimmed().isEmpty()?"PKR":currency->text().trimmed());service.setValue("receipt.footer",footer->text());service.setValue("backup.interval_hours",QString::number(backupHours->value()));service.setValue("printer.thermal_path",thermalPath->text().trimmed());QMessageBox::information(this,"Settings saved","Settings were saved to the local database. Restart the app to apply a changed automatic backup interval.");}catch(const std::exception& error){QMessageBox::critical(this,"Could not save settings",error.what());}});
    connect(testReceipt,&QPushButton::clicked,this,[this]{try{const auto path=pos::SettingsService(database_).value("printer.thermal_path");pos::ThermalPrintService::writeRaw(path,pos::ThermalPrintService::receiptBytes("Nexora POS","TEST-1",{{"Printer test",1,100}},100));QMessageBox::information(this,"Receipt sent","The ESC/POS test receipt was sent to the configured device.");}catch(const std::exception& error){QMessageBox::critical(this,"Could not print receipt",error.what());}});
    connect(testLabel,&QPushButton::clicked,this,[this]{try{const auto path=pos::SettingsService(database_).value("printer.thermal_path");pos::ThermalPrintService::writeRaw(path,pos::ThermalPrintService::barcodeLabelBytes("Nexora test label","123456789012"));QMessageBox::information(this,"Label sent","The Code128 test label was sent to the configured device.");}catch(const std::exception& error){QMessageBox::critical(this,"Could not print label",error.what());}});
    connect(setPin,&QPushButton::clicked,this,[this,pinStatus]{bool ok=false;const auto first=QInputDialog::getText(this,"Set security PIN","PIN (4-12 digits):",QLineEdit::Password,{},&ok);if(!ok)return;const auto second=QInputDialog::getText(this,"Set security PIN","Confirm PIN:",QLineEdit::Password,{},&ok);if(!ok||first!=second){QMessageBox::warning(this,"PIN not changed","The PIN confirmation did not match.");return;}try{pos::SecurityService(database_).setPin(first);pinStatus->setText("Sensitive-action PIN: configured");QMessageBox::information(this,"PIN saved","The security PIN is stored as a salted hash in the local database.");}catch(const std::exception& error){QMessageBox::critical(this,"Could not set PIN",error.what());}});
    connect(clearPin,&QPushButton::clicked,this,[this,pinStatus]{if(!authorizeSensitiveAction("clear the security PIN"))return;if(QMessageBox::question(this,"Clear security PIN","Remove the configured security PIN?")==QMessageBox::Yes){try{pos::SecurityService(database_).clearPin();pinStatus->setText("Sensitive-action PIN: not configured");}catch(const std::exception& error){QMessageBox::critical(this,"Could not clear PIN",error.what());}}});
    connect(notifications,&QPushButton::clicked,this,[this]{try{const auto items=pos::NotificationService(database_).unread();if(items.isEmpty()){QMessageBox::information(this,"Notifications","You have no unread notifications.");return;}QString text;for(const auto& item:items){text+=QString("[%1] %2\n%3\n\n").arg(item.createdAt,item.title,item.body);pos::NotificationService(database_).markRead(item.id);}QMessageBox::information(this,"Notifications",text.trimmed());}catch(const std::exception& error){QMessageBox::critical(this,"Could not load notifications",error.what());}});return page;
}
QWidget* MainWindow::makeAnalytics(){auto* page=new QWidget;auto* layout=new QVBoxLayout(page);layout->setContentsMargins(0,0,0,0);auto* title=new QLabel("Analytics",page);title->setObjectName("pageTitle");layout->addWidget(title);auto* metrics=new QTableWidget(page);metrics->setColumnCount(2);metrics->setRowCount(4);metrics->setHorizontalHeaderLabels({"KPI","Value (paisa)"});metrics->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);metrics->setEditTriggers(QAbstractItemView::NoEditTriggers);for(int row=0;row<4;++row)metrics->setItem(row,0,new QTableWidgetItem(QStringList{"7-day sales","7-day purchases","Receivables","Low-stock products"}[row]));layout->addWidget(metrics);auto* trend=new QTableWidget(page);trend->setColumnCount(2);trend->setHorizontalHeaderLabels({"Date","Sales (paisa)"});trend->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);trend->setEditTriggers(QAbstractItemView::NoEditTriggers);layout->addWidget(trend,1);auto* refresh=new QPushButton("Refresh dashboard",page);layout->addWidget(refresh);const auto load=[this,metrics,trend]{try{const auto end=QDate::currentDate();const auto start=end.addDays(-6);const auto report=pos::ReportService(database_).summary(start,end);const QList<qint64> values={report.sales,report.purchases,report.receivables,report.lowStock};for(int row=0;row<values.size();++row)metrics->setItem(row,1,new QTableWidgetItem(QString::number(values[row])));auto q=database_->prepare("SELECT substr(created_at,1,10),COALESCE(SUM(total_paisa),0) FROM sales WHERE status!='voided' AND created_at>=? AND created_at<? GROUP BY substr(created_at,1,10) ORDER BY substr(created_at,1,10)");q.bind(1,start.toString(Qt::ISODate));q.bind(2,end.addDays(1).toString(Qt::ISODate));trend->setRowCount(0);while(q.stepRow()){const int row=trend->rowCount();trend->insertRow(row);trend->setItem(row,0,new QTableWidgetItem(q.text(0)));trend->setItem(row,1,new QTableWidgetItem(QString::number(q.integer(1))));}}catch(const std::exception& error){QMessageBox::critical(this,"Could not load analytics",error.what());}};load();registerDataRefresh("sales",load);registerDataRefresh("purchases",load);registerDataRefresh("customers",load);registerDataRefresh("inventory",load);connect(refresh,&QPushButton::clicked,this,load);return page;}
QWidget* MainWindow::makePlaceholder(const QString& title,const QString& description){auto* page=new QWidget;auto* l=new QVBoxLayout(page);l->setContentsMargins(0,0,0,0);auto* h=new QLabel(title,page);h->setObjectName("pageTitle");auto* panel=new QFrame(page);panel->setObjectName("panel");auto* pl=new QVBoxLayout(panel);auto* text=new QLabel(description,panel);text->setObjectName("muted");text->setWordWrap(true);pl->addWidget(text);pl->addStretch();l->addWidget(h);l->addWidget(panel,1);return page;}
QWidget* MainWindow::makeBackupRestore(){
    auto* page=new QWidget; auto* layout=new QVBoxLayout(page); layout->setContentsMargins(0,0,0,0); layout->setSpacing(16);
    auto* title=new QLabel("Backup & Restore",page); title->setObjectName("pageTitle"); layout->addWidget(title);
    auto* explanation=new QLabel("Each backup is a verified, consistent SQLite snapshot. Restoring creates a safety backup of the current database first.",page); explanation->setObjectName("muted"); explanation->setWordWrap(true); layout->addWidget(explanation);
    auto* actions=new QHBoxLayout; auto* backup=new QPushButton("Create verified backup",page); backup->setObjectName("primary"); auto* restore=new QPushButton("Restore selected backup",page); restore->setObjectName("danger"); auto* restoreExternal=new QPushButton("Restore from drive/file",page); restoreExternal->setObjectName("danger"); actions->addWidget(backup); actions->addWidget(restore); actions->addWidget(restoreExternal); actions->addStretch(); layout->addLayout(actions);
    auto* table=new QTableWidget(page); table->setColumnCount(3); table->setHorizontalHeaderLabels({"Created","Status","File"}); table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch); table->setSelectionBehavior(QAbstractItemView::SelectRows); table->setEditTriggers(QAbstractItemView::NoEditTriggers); table->setAlternatingRowColors(true); layout->addWidget(table,1);
    const auto refresh=[this,table](){
        pos::BackupService service(database_); const auto backups=service.verifiedBackups(); table->setRowCount(backups.size());
        for(int row=0;row<backups.size();++row){ const auto& item=backups[row]; auto* created=new QTableWidgetItem(item.verifiedAt); created->setData(Qt::UserRole,QString::fromStdWString(item.file.wstring())); table->setItem(row,0,created); table->setItem(row,1,new QTableWidgetItem(item.status)); table->setItem(row,2,new QTableWidgetItem(QString::fromStdWString(item.file.wstring()))); }
    };
    refresh();
    connect(backup,&QPushButton::clicked,this,[this,refresh]{ try { const auto selected=QFileDialog::getExistingDirectory(this,"Select backup destination (local or USB drive)",QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)); if(selected.isEmpty())return; const auto folder=selected+"/nexora-backups"; const auto file=std::filesystem::path(folder.toStdWString())/(L"backup-"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss").toStdWString()+L".db"); pos::BackupService service(database_); service.createVerifiedBackup(file); service.pruneVerifiedBackups(30); refresh(); QMessageBox::information(this,"Backup complete",QString("Verified backup saved to:\n%1").arg(QString::fromStdWString(file.wstring()))); } catch(const std::exception& error) { QMessageBox::critical(this,"Backup failed",error.what()); }});
    connect(restore,&QPushButton::clicked,this,[this,table,refresh]{ const int row=table->currentRow(); if(row<0){QMessageBox::information(this,"Select a backup","Select a verified backup to restore.");return;} const auto selected=std::filesystem::path(table->item(row,0)->data(Qt::UserRole).toString().toStdWString()); if(QMessageBox::warning(this,"Restore business data",QString("This replaces the current database with:\n%1\n\nA safety backup of the current database will be created first.").arg(QString::fromStdWString(selected.wstring())),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return; if(!authorizeSensitiveAction("restore business data"))return; try{ const auto safety=std::filesystem::path(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdWString())/L"backups"/(L"before-restore-"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss").toStdWString()+L".db"); pos::BackupService service(database_); service.restoreVerifiedBackup(selected,safety); refresh(); QMessageBox::information(this,"Restore complete","The database passed its integrity check. Restart Nexora POS before taking new sales."); }catch(const std::exception& error){QMessageBox::critical(this,"Restore failed",error.what());}});
    connect(restoreExternal,&QPushButton::clicked,this,[this,refresh]{const auto fileName=QFileDialog::getOpenFileName(this,"Select verified backup from a drive",{},"SQLite backups (*.db)");if(fileName.isEmpty())return;if(!authorizeSensitiveAction("restore business data"))return;const auto selected=std::filesystem::path(fileName.toStdWString());if(QMessageBox::warning(this,"Restore business data",QString("Restore this backup?\n%1\n\nA safety backup is created first.").arg(fileName),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;try{const auto safety=std::filesystem::path(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdWString())/L"backups"/(L"before-restore-"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss").toStdWString()+L".db");pos::BackupService(database_).restoreVerifiedBackup(selected,safety);refresh();QMessageBox::information(this,"Restore complete","The selected backup passed integrity verification. Restart Nexora POS before taking new sales.");}catch(const std::exception& error){QMessageBox::critical(this,"Restore failed",error.what());}});
    return page;
}
void MainWindow::switchTheme(){dark_=!dark_;qApp->setStyleSheet(dark_?darkStyle:lightStyle);}
