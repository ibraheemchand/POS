#pragma once
#include "core/types.h"
#include <memory>
namespace pos { class Database; struct ShiftClose { Money expected{}; Money counted{}; Money difference{}; }; class ShiftService { public: explicit ShiftService(std::shared_ptr<Database> db); QString open(Money openingCash); ShiftClose close(const QString& shiftId, Money countedCash); private: std::shared_ptr<Database> db_; }; }
