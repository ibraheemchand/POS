#include "core/customer_service.h"
#include "core/database.h"
namespace pos {
CustomerService::CustomerService(std::shared_ptr<Database> database):db_(std::move(database)){}
QString CustomerService::create(const Customer& customer){if(customer.name.trimmed().isEmpty()||customer.creditLimit<0||customer.paymentTermsDays<0)throw DatabaseError("invalid customer");Transaction tx(db_->handle());const auto id=uuid();auto q=db_->prepare("INSERT INTO customers(id,name,phone,credit_limit_paisa,payment_terms_days,created_at) VALUES(?,?,?,?,?,?)");q.bind(1,id);q.bind(2,customer.name.trimmed());q.bind(3,customer.phone.trimmed());q.bind(4,customer.creditLimit);q.bind(5,customer.paymentTermsDays);q.bind(6,utcNow());q.execute();tx.commit();return id;}
void CustomerService::archive(const QString& id){Transaction tx(db_->handle());auto q=db_->prepare("UPDATE customers SET is_deleted=1 WHERE id=? AND is_deleted=0");q.bind(1,id);q.execute();if(sqlite3_changes(db_->handle())!=1)throw DatabaseError("customer not found");tx.commit();}
}
