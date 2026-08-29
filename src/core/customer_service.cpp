#include "core/customer_service.h"
#include "core/database.h"
#include "core/data_change_bus.h"
namespace pos {
CustomerService::CustomerService(std::shared_ptr<Database> database):db_(std::move(database)){}
QString CustomerService::create(const Customer& customer){if(customer.name.trimmed().isEmpty()||customer.creditLimit<0||customer.paymentTermsDays<0)throw DatabaseError("invalid customer");Transaction tx(db_->handle());const auto id=uuid();auto q=db_->prepare("INSERT INTO customers(id,name,phone,credit_limit_paisa,payment_terms_days,created_at) VALUES(?,?,?,?,?,?)");q.bind(1,id);q.bind(2,customer.name.trimmed());q.bind(3,customer.phone.trimmed());q.bind(4,customer.creditLimit);q.bind(5,customer.paymentTermsDays);q.bind(6,utcNow());q.execute();tx.commit();notifyCustomersChanged();return id;}
void CustomerService::archive(const QString& id){Transaction tx(db_->handle());auto q=db_->prepare("UPDATE customers SET is_deleted=1 WHERE id=? AND is_deleted=0");q.bind(1,id);q.execute();if(sqlite3_changes(db_->handle())!=1)throw DatabaseError("customer not found");tx.commit();notifyCustomersChanged();}

void CustomerService::update(const Customer& customer) {
    if(customer.name.trimmed().isEmpty() || customer.creditLimit < 0 || customer.paymentTermsDays < 0)
        throw DatabaseError("invalid customer");
    Transaction tx(db_->handle());
    auto q = db_->prepare("UPDATE customers SET name=?, phone=?, credit_limit_paisa=?, payment_terms_days=? WHERE id=? AND is_deleted=0");
    q.bind(1, customer.name.trimmed());
    q.bind(2, customer.phone.trimmed());
    q.bind(3, customer.creditLimit);
    q.bind(4, static_cast<qint64>(customer.paymentTermsDays));
    q.bind(5, customer.id);
    q.execute();
    if(sqlite3_changes(db_->handle()) != 1)
        throw DatabaseError("customer not found");
    tx.commit();
    notifyCustomersChanged();
}

QList<Customer> CustomerService::listActive() const {
    QList<Customer> result;
    auto q = db_->prepare("SELECT id, name, phone, credit_limit_paisa, balance_paisa, payment_terms_days FROM customers WHERE is_deleted=0 ORDER BY name");
    while (q.stepRow()) {
        result.append({q.text(0), q.text(1), q.text(2), q.integer(3), q.integer(4), static_cast<int>(q.integer(5)), false});
    }
    return result;
}

QList<InvoiceSummary> CustomerService::unpaidInvoices(const QString& customerId) const {
    QList<InvoiceSummary> result;
    auto invoices = db_->prepare("SELECT id, invoice_no, due_paisa FROM sales WHERE customer_id=? AND due_paisa>0 AND status!='voided' ORDER BY created_at");
    invoices.bind(1, customerId);
    while (invoices.stepRow()) {
        result.append({invoices.text(0), invoices.text(1), invoices.integer(2)});
    }
    return result;
}

Money CustomerService::totalReceivables() const {
    auto r = db_->prepare("SELECT COALESCE(SUM(balance_paisa), 0) FROM customers WHERE is_deleted=0");
    r.stepRow();
    return r.integer(0);
}
}

