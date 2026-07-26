#pragma once
#include "core/types.h"
#include <memory>
namespace pos { class Database; struct BusinessSummary { Money sales{},purchases{},receivables{},payables{},inventoryValue{}; qint64 lowStock{}; }; class ReportService { public: explicit ReportService(std::shared_ptr<Database> db); BusinessSummary summary(const QDate& from,const QDate& to) const; private: std::shared_ptr<Database> db_; }; }
