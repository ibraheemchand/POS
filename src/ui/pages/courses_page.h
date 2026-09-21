#pragma once

#include <QWidget>
#include <memory>
#include <QTableWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

namespace pos { class Database; }

class CoursesPage : public QWidget {
    Q_OBJECT
public:
    explicit CoursesPage(std::shared_ptr<pos::Database> database, QWidget* parent = nullptr);
public slots:
    void load();
private:
    std::shared_ptr<pos::Database> database_;
    QLineEdit* search_{};
    QTableWidget* table_{};
    QPushButton* addBtn_{};
    QPushButton* editBtn_{};
    QPushButton* archiveBtn_{};
    QPushButton* refreshBtn_{};
    QLabel* feedbackLabel_{};
};
