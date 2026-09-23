#include "ui/pages/profit_report_page.h"
#include "ui/pages/page_helper.h"
#include "core/database.h"
#include "core/partner_service.h"
#include "core/auth_session.h"
#include "core/excel_export_service.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QPushButton>
#include <QTimer>

namespace {
QFrame* makeCard(QWidget* parent, const QString& label, QLabel*& valueOut) {
    auto* card = new QFrame(parent);
    card->setObjectName("metric");
    auto* l = new QVBoxLayout(card);
    l->setContentsMargins(14, 12, 14, 12);
    auto* name = new QLabel(label, card);
    name->setObjectName("metricLabel");
    valueOut = new QLabel("PKR 0.00", card);
    valueOut->setObjectName("metricValue");
    l->addWidget(name);
    l->addWidget(valueOut);
    return card;
}

QTableWidget* makeTable(const QStringList& headers) {
    auto* table = new QTableWidget;
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    return table;
}
} // namespace

ProfitReportPage::ProfitReportPage(std::shared_ptr<pos::Database> database, QWidget* parent)
    : QWidget(parent), database_(std::move(database)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* title = new QLabel("Profit & Commission", this);
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    lockedLabel_ = new QLabel("This section is locked. Re-open it with the owner PIN to view profits.", this);
    lockedLabel_->setObjectName("muted");
    lockedLabel_->setWordWrap(true);
    lockedLabel_->hide();
    layout->addWidget(lockedLabel_);

    auto* controls = new QHBoxLayout;
    fromDate_ = new QDateEdit(QDate::currentDate().addDays(-QDate::currentDate().day() + 1), this); // month start
    toDate_ = new QDateEdit(QDate::currentDate(), this);
    fromDate_->setCalendarPopup(true);
    toDate_->setCalendarPopup(true);
    auto* todayBtn = new QPushButton("Today", this);
    auto* monthBtn = new QPushButton("This month", this);
    auto* exportBtn = new QPushButton("Export current tab", this);
    auto* refreshBtn = new QPushButton("&Refresh", this);
    refreshBtn->setObjectName("primary");
    controls->addWidget(new QLabel("From", this));
    controls->addWidget(fromDate_);
    controls->addWidget(new QLabel("To", this));
    controls->addWidget(toDate_);
    controls->addWidget(todayBtn);
    controls->addWidget(monthBtn);
    controls->addStretch();
    controls->addWidget(exportBtn);
    controls->addWidget(refreshBtn);
    layout->addLayout(controls);

    auto* cards = new QHBoxLayout;
    cards->addWidget(makeCard(this, "Total sales value", cardSales_));
    cards->addWidget(makeCard(this, "Total commission", cardCommission_));
    cards->addWidget(makeCard(this, "Total partner share", cardPartner_));
    cards->addWidget(makeCard(this, "MY TOTAL PROFIT", cardProfit_));
    cards->addWidget(makeCard(this, "Paid to partners", cardPaid_));
    cards->addWidget(makeCard(this, "Still owed", cardOwed_));
    layout->addLayout(cards);

    tabs_ = new QTabWidget(this);
    courseTable_ = makeTable({"Course", "Partner", "Times sold", "Sale value", "Commission", "Partner share", "My profit"});
    partnerTable_ = makeTable({"Partner", "Share earned", "Paid out", "Balance owed"});
    bookTable_ = makeTable({"Book", "Partner", "Times sold", "Sale value", "Commission", "Partner share", "My profit"});
    profitsTable_ = makeTable({"Date", "Invoice", "Source", "Name", "Sale value", "My profit"});
    tabs_->addTab(courseTable_, "Per course");
    tabs_->addTab(partnerTable_, "Per partner");
    tabs_->addTab(bookTable_, "Standalone books");
    tabs_->addTab(profitsTable_, "My Profits");
    layout->addWidget(tabs_, 1);

    connect(refreshBtn, &QPushButton::clicked, this, [this] { load(); });
    connect(fromDate_, &QDateEdit::dateChanged, this, [this] { load(); });
    connect(toDate_, &QDateEdit::dateChanged, this, [this] { load(); });
    connect(todayBtn, &QPushButton::clicked, this, [this] { fromDate_->setDate(QDate::currentDate()); toDate_->setDate(QDate::currentDate()); });
    connect(monthBtn, &QPushButton::clicked, this, [this] { fromDate_->setDate(QDate(QDate::currentDate().year(), QDate::currentDate().month(), 1)); toDate_->setDate(QDate::currentDate()); });
    connect(exportBtn, &QPushButton::clicked, this, [this] { exportCurrentTab(); });

    QTimer::singleShot(0, this, [this] { load(); });
}

bool ProfitReportPage::ensureUnlocked() { return pos::unlockOwnerSession(this, database_); }

void ProfitReportPage::load() {
    auto& session = pos::AuthSession::instance();
    const bool unlocked = session.isUnlocked();
    lockedLabel_->setVisible(!unlocked);
    tabs_->setVisible(unlocked);
    if (!unlocked) {
        for (auto* t : {courseTable_, partnerTable_, bookTable_, profitsTable_}) t->setRowCount(0);
        for (auto* c : {cardSales_, cardCommission_, cardPartner_, cardProfit_, cardPaid_, cardOwed_}) c->setText("—");
        return;
    }
    session.touch();
    const auto from = fromDate_->date();
    const auto to = toDate_->date();
    try {
        pos::PartnerService service(database_);
        const auto sum = service.summary(from, to);
        cardSales_->setText("PKR " + pos::formatPaisa(sum.totalSales));
        cardCommission_->setText("PKR " + pos::formatPaisa(sum.totalCommission));
        cardPartner_->setText("PKR " + pos::formatPaisa(sum.totalPartner));
        cardProfit_->setText("PKR " + pos::formatPaisa(sum.myProfit));
        cardPaid_->setText("PKR " + pos::formatPaisa(sum.totalPaidOut));
        cardOwed_->setText("PKR " + pos::formatPaisa(sum.totalOwed));

        const auto fillSource = [](QTableWidget* table, const QList<pos::SourceReportRow>& rows) {
            table->setRowCount(0);
            for (const auto& r : rows) {
                const int row = table->rowCount();
                table->insertRow(row);
                table->setItem(row, 0, new QTableWidgetItem(r.name));
                table->setItem(row, 1, new QTableWidgetItem(r.partnerName));
                table->setItem(row, 2, new QTableWidgetItem(QString::number(r.timesSold)));
                table->setItem(row, 3, new QTableWidgetItem("PKR " + pos::formatPaisa(r.saleValue)));
                table->setItem(row, 4, new QTableWidgetItem("PKR " + pos::formatPaisa(r.commission)));
                table->setItem(row, 5, new QTableWidgetItem("PKR " + pos::formatPaisa(r.partnerAmount)));
                table->setItem(row, 6, new QTableWidgetItem("PKR " + pos::formatPaisa(r.myProfit)));
            }
        };
        fillSource(courseTable_, service.courseReport(from, to));
        fillSource(bookTable_, service.standaloneBookReport(from, to));

        partnerTable_->setRowCount(0);
        for (const auto& p : service.partnerReport(from, to)) {
            const int row = partnerTable_->rowCount();
            partnerTable_->insertRow(row);
            partnerTable_->setItem(row, 0, new QTableWidgetItem(p.name));
            partnerTable_->setItem(row, 1, new QTableWidgetItem("PKR " + pos::formatPaisa(p.earned)));
            partnerTable_->setItem(row, 2, new QTableWidgetItem("PKR " + pos::formatPaisa(p.paidOut)));
            partnerTable_->setItem(row, 3, new QTableWidgetItem("PKR " + pos::formatPaisa(p.balance)));
        }

        profitsTable_->setRowCount(0);
        for (const auto& e : service.profitEntries(from, to)) {
            const int row = profitsTable_->rowCount();
            profitsTable_->insertRow(row);
            profitsTable_->setItem(row, 0, new QTableWidgetItem(e.createdAt.left(10)));
            profitsTable_->setItem(row, 1, new QTableWidgetItem(e.invoiceNo));
            profitsTable_->setItem(row, 2, new QTableWidgetItem(e.reversal ? e.sourceType + " (reversal)" : e.sourceType));
            profitsTable_->setItem(row, 3, new QTableWidgetItem(e.sourceName));
            profitsTable_->setItem(row, 4, new QTableWidgetItem("PKR " + pos::formatPaisa(e.saleValue)));
            profitsTable_->setItem(row, 5, new QTableWidgetItem("PKR " + pos::formatPaisa(e.myProfit)));
        }
    } catch (const std::exception&) {
        lockedLabel_->show();
        tabs_->hide();
    }
}

void ProfitReportPage::exportCurrentTab() {
    if (!pos::AuthSession::instance().isUnlocked()) { QMessageBox::warning(this, "Locked", "Unlock the section first."); return; }
    auto* table = qobject_cast<QTableWidget*>(tabs_->currentWidget());
    if (!table || table->rowCount() == 0) { QMessageBox::information(this, "Export", "Nothing to export in this tab."); return; }
    const auto file = QFileDialog::getSaveFileName(this, "Export report", tabs_->tabText(tabs_->currentIndex()) + ".xlsx", "Excel workbook (*.xlsx)");
    if (file.isEmpty()) return;
    QStringList headers;
    for (int c = 0; c < table->columnCount(); ++c) headers << table->horizontalHeaderItem(c)->text();
    QList<QStringList> rows;
    for (int r = 0; r < table->rowCount(); ++r) {
        QStringList cells;
        for (int c = 0; c < table->columnCount(); ++c) cells << (table->item(r, c) ? table->item(r, c)->text() : QString());
        rows << cells;
    }
    try { pos::ExcelExportService::writeWorkbook(file, headers, rows); QMessageBox::information(this, "Export", "Saved."); }
    catch (const std::exception& e) { QMessageBox::critical(this, "Export failed", e.what()); }
}
