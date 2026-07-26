#pragma once
#include "core/types.h"
#include <memory>
namespace pos { class Database; struct Cheque { QString id,direction,partyId,number,bank,status; Money amount{}; QDate dueDate; }; class ChequeService { public: explicit ChequeService(std::shared_ptr<Database> db); QString record(const Cheque& cheque); void setStatus(const QString& id,const QString& status); QList<Cheque> dueBy(const QDate& date) const; private: std::shared_ptr<Database> db_; }; }
