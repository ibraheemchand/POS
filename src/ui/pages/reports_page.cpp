#include "ui/pages/reports_page.h"
#include "ui/pages/page_helper.h"
#include "core/database.h"
#include "core/report_service.h"
#include "core/commission_service.h"
#include "core/excel_export_service.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QHeaderView>
#include <QPrinter>
#include <QPrintDialog>
#include <QPainter>
#include <QTimer>

ReportsPage::ReportsPage(std::shared_ptr<pos::Database> database, QWidget* parent)
    : QWidget(parent), database_(database) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* title = new QLabel("Reports", this);
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    auto* filters = new QHBoxLayout;
    from_ = new QDateEdit(QDate::currentDate().addDays(-30), this);
    to_ = new QDateEdit(QDate::currentDate(), this);
    from_->setCalendarPopup(true);
    to_->setCalendarPopup(true);

    refreshBtn_ = new QPushButton("Run report", this);
    refreshBtn_->setObjectName("primary");
    
    exportCsvBtn_ = new QPushButton("Export CSV", this);
    exportPdfBtn_ = new QPushButton("Export PDF", this);
    exportExcelBtn_ = new QPushButton("Export Excel", this);
    printBtn_ = new QPushButton("Print A4", this);

    filters->addWidget(new QLabel("From", this));
    filters->addWidget(from_);
    filters->addWidget(new QLabel("To", this));
    filters->addWidget(to_);
    filters->addWidget(refreshBtn_);
    filters->addWidget(exportCsvBtn_);
    filters->addWidget(exportPdfBtn_);
    filters->addWidget(exportExcelBtn_);
    filters->addWidget(printBtn_);
    filters->addStretch();
    layout->addLayout(filters);

    summary_ = new QTableWidget(this);
    summary_->setColumnCount(2);
    summary_->setRowCount(6);
    summary_->setHorizontalHeaderLabels({"Metric", "Value"});
    summary_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    summary_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    const QStringList labels = {"Sales", "Purchases", "Receivables", "Payables", "Inventory value", "Low-stock products"};
    for (int row = 0; row < labels.size(); ++row) {
        summary_->setItem(row, 0, new QTableWidgetItem(labels[row]));
    }
    layout->addWidget(summary_, 1);

    auto* commissionTitle = new QLabel("Book commission split", this);
    commissionTitle->setObjectName("sectionTitle");
    layout->addWidget(commissionTitle);

    commissionSummary_ = new QTableWidget(this);
    commissionSummary_->setColumnCount(2);
    commissionSummary_->setRowCount(6);
    commissionSummary_->setHorizontalHeaderLabels({"Metric", "Value"});
    commissionSummary_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    commissionSummary_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    const QStringList commissionLabels = {"Revenue (net of discounts)", "Commission pool", "Partner accrued", "Owner profit", "Discounts given", "Manager overrides used"};
    for (int row = 0; row < commissionLabels.size(); ++row) {
        commissionSummary_->setItem(row, 0, new QTableWidgetItem(commissionLabels[row]));
    }
    layout->addWidget(commissionSummary_, 1);

    // Initial setups
    QTimer::singleShot(0, this, [this] { load(); });

    // Connections
    connect(refreshBtn_, &QPushButton::clicked, this, &ReportsPage::load);
    connect(exportCsvBtn_, &QPushButton::clicked, this, &ReportsPage::exportReport);
    connect(exportPdfBtn_, &QPushButton::clicked, this, &ReportsPage::exportPdfFile);
    connect(exportExcelBtn_, &QPushButton::clicked, this, &ReportsPage::exportExcelFile);
    connect(printBtn_, &QPushButton::clicked, this, &ReportsPage::printReport);

    // Set tab order
    setTabOrder(from_, to_);
    setTabOrder(to_, refreshBtn_);
    setTabOrder(refreshBtn_, exportCsvBtn_);
    setTabOrder(exportCsvBtn_, exportPdfBtn_);
    setTabOrder(exportPdfBtn_, exportExcelBtn_);
    setTabOrder(exportExcelBtn_, printBtn_);
    setTabOrder(printBtn_, summary_);
}

void ReportsPage::load() {
    try {
        const auto r = pos::ReportService(database_).summary(from_->date(), to_->date());
        const QList<qint64> values = {r.sales, r.purchases, r.receivables, r.payables, r.inventoryValue, r.lowStock};
        for (int row = 0; row < values.size(); ++row) {
            if (row == 5) {
                summary_->setItem(row, 1, new QTableWidgetItem(QString::number(values[row])));
            } else {
                summary_->setItem(row, 1, new QTableWidgetItem("PKR " + pos::formatPaisa(values[row])));
            }
        }

        const auto c = pos::CommissionService(database_).totals(from_->date(), to_->date());
        const QList<qint64> commissionValues = {c.revenue, c.commissionPool, c.partnerAccrued, c.ownerProfit, c.discountsGiven, c.overrideCount};
        for (int row = 0; row < commissionValues.size(); ++row) {
            if (row == 5) {
                commissionSummary_->setItem(row, 1, new QTableWidgetItem(QString::number(commissionValues[row])));
            } else {
                commissionSummary_->setItem(row, 1, new QTableWidgetItem("PKR " + pos::formatPaisa(commissionValues[row])));
            }
        }
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Could not run report", error.what());
    }
}

void ReportsPage::exportReport() {
    const auto fileName = QFileDialog::getSaveFileName(this, "Export report", {}, "CSV files (*.csv)");
    if (fileName.isEmpty()) return;
    try {
        const auto r = pos::ReportService(database_).summary(from_->date(), to_->date());
        const QStringList labels = {"Sales", "Purchases", "Receivables", "Payables", "Inventory value", "Low-stock products"};
        const QList<qint64> values = {r.sales, r.purchases, r.receivables, r.payables, r.inventoryValue, r.lowStock};
        
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) throw pos::DatabaseError("could not open export file");
        QTextStream stream(&file);
        stream << "Metric,Value\n";
        for (int row = 0; row < labels.size(); ++row) {
            if (row == 5) {
                stream << labels[row] << "," << values[row] << "\n";
            } else {
                stream << labels[row] << ",\"PKR " << pos::formatPaisa(values[row]) << "\"\n";
            }
        }
        file.close();
        QMessageBox::information(this, "Report exported", "The report was exported to CSV.");
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Could not export report", error.what());
    }
}

void ReportsPage::exportPdfFile() {
    const auto fileName = QFileDialog::getSaveFileName(this, "Export report", {}, "PDF files (*.pdf)");
    if (fileName.isEmpty()) return;
    try {
        const auto r = pos::ReportService(database_).summary(from_->date(), to_->date());
        const QStringList labels = {"Sales", "Purchases", "Receivables", "Payables", "Inventory value", "Low-stock products"};
        const QList<qint64> values = {r.sales, r.purchases, r.receivables, r.payables, r.inventoryValue, r.lowStock};

        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(fileName);
        QPainter painter(&printer);
        painter.setFont(QFont("Arial", 14));
        painter.drawText(100, 100, "Nexora POS business report");
        painter.setFont(QFont("Arial", 10));
        painter.drawText(100, 130, QString("Period: %1 to %2").arg(from_->date().toString(Qt::ISODate), to_->date().toString(Qt::ISODate)));
        int y = 180;
        for (int row = 0; row < labels.size(); ++row) {
            QString valStr = (row == 5) ? QString::number(values[row]) : ("PKR " + pos::formatPaisa(values[row]));
            painter.drawText(100, y, QString("%1: %2").arg(labels[row]).arg(valStr));
            y += 30;
        }
        painter.end();
        QMessageBox::information(this, "PDF exported", "The report was exported to PDF.");
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Could not export PDF", error.what());
    }
}

void ReportsPage::exportExcelFile() {
    const auto fileName = QFileDialog::getSaveFileName(this, "Export report", {}, "Excel workbooks (*.xlsx)");
    if (fileName.isEmpty()) return;
    try {
        const auto r = pos::ReportService(database_).summary(from_->date(), to_->date());
        const QStringList labels = {"Sales", "Purchases", "Receivables", "Payables", "Inventory value", "Low-stock products"};
        const QList<qint64> values = {r.sales, r.purchases, r.receivables, r.payables, r.inventoryValue, r.lowStock};

        QList<QStringList> rows;
        for (int row = 0; row < labels.size(); ++row) {
            QString valStr = (row == 5) ? QString::number(values[row]) : ("PKR " + pos::formatPaisa(values[row]));
            rows.append({labels[row], valStr});
        }
        pos::ExcelExportService::writeWorkbook(fileName, {"Metric", "Value"}, rows);
        QMessageBox::information(this, "Report exported", "The report was exported to Excel.");
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Could not export report", error.what());
    }
}

void ReportsPage::printReport() {
    try {
        const auto r = pos::ReportService(database_).summary(from_->date(), to_->date());
        const QStringList labels = {"Sales", "Purchases", "Receivables", "Payables", "Inventory value", "Low-stock products"};
        const QList<qint64> values = {r.sales, r.purchases, r.receivables, r.payables, r.inventoryValue, r.lowStock};

        QPrinter printer(QPrinter::HighResolution);
        QPrintDialog dialog(&printer, this);
        if (dialog.exec() != QDialog::Accepted) return;
        
        QPainter painter(&printer);
        painter.setFont(QFont("Arial", 14));
        painter.drawText(100, 100, "Nexora POS business report");
        painter.setFont(QFont("Arial", 10));
        painter.drawText(100, 130, QString("Period: %1 to %2").arg(from_->date().toString(Qt::ISODate), to_->date().toString(Qt::ISODate)));
        int y = 180;
        for (int row = 0; row < labels.size(); ++row) {
            QString valStr = (row == 5) ? QString::number(values[row]) : ("PKR " + pos::formatPaisa(values[row]));
            painter.drawText(100, y, QString("%1: %2").arg(labels[row]).arg(valStr));
            y += 28;
        }
        painter.end();
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Could not print report", error.what());
    }
}
