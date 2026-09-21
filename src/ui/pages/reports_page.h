#pragma once

#include <QWidget>
#include <memory>
#include <QTableWidget>
#include <QDateEdit>
#include <QPushButton>

namespace pos { class Database; }

class ReportsPage : public QWidget {
    Q_OBJECT
public:
    explicit ReportsPage(std::shared_ptr<pos::Database> database, QWidget* parent = nullptr);
public slots:
    void load();
private:
    void exportReport();
    void exportPdfFile();
    void exportExcelFile();
    void printReport();

    std::shared_ptr<pos::Database> database_;
    QDateEdit* from_{};
    QDateEdit* to_{};
    QTableWidget* summary_{};
    QTableWidget* commissionSummary_{};
    QPushButton* refreshBtn_{};
    QPushButton* exportCsvBtn_{};
    QPushButton* exportPdfBtn_{};
    QPushButton* exportExcelBtn_{};
    QPushButton* printBtn_{};
};
