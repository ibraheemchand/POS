#include "core/payment_service.h"
#include "core/database.h"
#include <QStringList>

namespace pos {
PaymentService::PaymentService(std::shared_ptr<Database> database) : db_(std::move(database)) {}

PaymentResult PaymentService::recordCustomerPayment(const QString& customerId, const QList<InvoicePayment>& allocations, const QString& method, const QString& note) {
    if(customerId.isEmpty() || allocations.isEmpty()) throw DatabaseError("a customer payment requires an allocation");
    const QStringList methods{"cash","cheque","mobile_wallet","bank"};
    if(!methods.contains(method)) throw DatabaseError("unsupported customer payment method");
    Transaction tx(db_->handle());
    auto customer=db_->prepare("SELECT balance_paisa FROM customers WHERE id=? AND is_deleted=0"); customer.bind(1,customerId);
    if(!customer.stepRow()) throw DatabaseError("customer not found or archived");
    Money total{};
    for(const auto& allocation:allocations) {
        if(allocation.saleId.isEmpty() || allocation.amount<=0) throw DatabaseError("invalid payment allocation");
        auto sale=db_->prepare("SELECT due_paisa,status FROM sales WHERE id=? AND customer_id=?"); sale.bind(1,allocation.saleId); sale.bind(2,customerId);
        if(!sale.stepRow() || sale.text(1)=="voided") throw DatabaseError("invoice is unavailable for payment");
        if(allocation.amount>sale.integer(0)) throw DatabaseError("payment exceeds the invoice balance");
        auto update=db_->prepare("UPDATE sales SET due_paisa=due_paisa-?, paid_paisa=paid_paisa+?, status=CASE WHEN due_paisa-?=0 THEN 'paid' ELSE status END WHERE id=?"); update.bind(1,allocation.amount); update.bind(2,allocation.amount); update.bind(3,allocation.amount); update.bind(4,allocation.saleId); update.execute();
        total+=allocation.amount;
    }
    if(total>customer.integer(0)) throw DatabaseError("payment exceeds customer balance");
    const auto paymentId=uuid();
    auto payment=db_->prepare("INSERT INTO customer_payments(id,customer_id,method,amount_paisa,note,created_at) VALUES(?,?,?,?,?,?)"); payment.bind(1,paymentId); payment.bind(2,customerId); payment.bind(3,method); payment.bind(4,total); payment.bind(5,note); payment.bind(6,utcNow()); payment.execute();
    for(const auto& allocation:allocations) { auto link=db_->prepare("INSERT INTO customer_payment_allocations(id,payment_id,sale_id,amount_paisa) VALUES(?,?,?,?)"); link.bind(1,uuid()); link.bind(2,paymentId); link.bind(3,allocation.saleId); link.bind(4,allocation.amount); link.execute(); }
    auto balance=db_->prepare("UPDATE customers SET balance_paisa=balance_paisa-? WHERE id=?"); balance.bind(1,total); balance.bind(2,customerId); balance.execute();
    auto ledger=db_->prepare("INSERT INTO customer_ledger(id,customer_id,description,debit_paisa,credit_paisa,running_balance_paisa,created_at) SELECT ?,?,'Customer payment',0,?,balance_paisa,? FROM customers WHERE id=?"); ledger.bind(1,uuid()); ledger.bind(2,customerId); ledger.bind(3,total); ledger.bind(4,utcNow()); ledger.bind(5,customerId); ledger.execute();
    if(method=="cash") { auto cash=db_->prepare("INSERT INTO cash_transactions(id,type,amount_paisa,reason,created_at) VALUES(?,?,?,?,?)"); cash.bind(1,uuid()); cash.bind(2,"cash_in"); cash.bind(3,total); cash.bind(4,"Customer payment"); cash.bind(5,utcNow()); cash.execute(); }
    tx.commit(); return {paymentId,total};
}
} // namespace pos
