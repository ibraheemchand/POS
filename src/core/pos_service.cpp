#include "core/pos_service.h"
#include "core/database.h"
#include <algorithm>
#include <QStringList>

namespace pos {
PosService::PosService(std::shared_ptr<Database> database) : db_(std::move(database)) {}

QList<PosService::BatchAllocation> PosService::allocateBatches(const SaleLine& line, const QString& saleId, const QString& performedBy) {
    auto product=db_->prepare("SELECT stock_quantity, track_batches FROM products WHERE id=? AND is_deleted=0");
    product.bind(1,line.productId);
    if (!product.stepRow()) throw DatabaseError("product not found or archived");
    const auto stock=product.integer(0); const bool tracks=product.integer(1)!=0;
    if (line.quantity<=0 || stock<line.quantity) throw DatabaseError("insufficient stock");
    QList<BatchAllocation> allocations;
    if (tracks) {
        if (!line.batchId.isEmpty()) {
            auto selected=db_->prepare("SELECT quantity_remaining FROM batches WHERE id=? AND product_id=? AND quantity_remaining>=? AND (expiry_date IS NULL OR expiry_date>=?)");
            selected.bind(1,line.batchId); selected.bind(2,line.productId); selected.bind(3,line.quantity); selected.bind(4,QDate::currentDate().toString(Qt::ISODate));
            if (!selected.stepRow()) throw DatabaseError("selected batch is expired or has insufficient stock");
            allocations.append({line.batchId,line.quantity});
        } else {
            auto fefo=db_->prepare("SELECT id, quantity_remaining FROM batches WHERE product_id=? AND quantity_remaining>0 AND (expiry_date IS NULL OR expiry_date>=?) ORDER BY CASE WHEN expiry_date IS NULL THEN 1 ELSE 0 END, expiry_date, created_at");
            fefo.bind(1,line.productId); fefo.bind(2,QDate::currentDate().toString(Qt::ISODate));
            Quantity remaining=line.quantity;
            while (remaining>0 && fefo.stepRow()) {
                const auto take=std::min(remaining,fefo.integer(1));
                allocations.append({fefo.text(0),take});
                remaining-=take;
            }
            if (remaining>0) throw DatabaseError("no eligible batches have sufficient stock");
        }
        for (const auto& allocation: allocations) {
            auto decrement=db_->prepare("UPDATE batches SET quantity_remaining=quantity_remaining-? WHERE id=? AND product_id=? AND quantity_remaining>=?");
            decrement.bind(1,allocation.quantity); decrement.bind(2,allocation.batchId); decrement.bind(3,line.productId); decrement.bind(4,allocation.quantity); decrement.execute();
            if (sqlite3_changes(db_->handle())!=1) throw DatabaseError("batch stock changed; retry sale");
        }
    }
    auto update=db_->prepare("UPDATE products SET stock_quantity=stock_quantity-?, updated_at=? WHERE id=? AND stock_quantity>=?");
    update.bind(1,line.quantity); update.bind(2,utcNow()); update.bind(3,line.productId); update.bind(4,line.quantity); update.execute();
    if (sqlite3_changes(db_->handle())!=1) throw DatabaseError("stock changed; retry sale");
    if (!tracks) allocations.append({{},line.quantity});
    Quantity runningBalance=stock;
    for (const auto& allocation: allocations) {
        runningBalance-=allocation.quantity;
        auto movement=db_->prepare("INSERT INTO stock_movements(id,product_id,batch_id,type,quantity,original_unit,reference_id,reason,balance_after,performed_by,created_at) VALUES(?,?,?,?,?,?,?,?,?,?,?)");
        movement.bind(1,uuid()); movement.bind(2,line.productId); if(allocation.batchId.isEmpty()) movement.bindNull(3); else movement.bind(3,allocation.batchId);
        movement.bind(4,"sale"); movement.bind(5,-allocation.quantity); movement.bind(6,line.unitName); movement.bind(7,saleId); movement.bind(8,"Completed sale"); movement.bind(9,runningBalance); movement.bind(10,performedBy); movement.bind(11,utcNow()); movement.execute();
    }
    return allocations;
}

SaleResult PosService::completeSale(const SaleRequest& request) {
    if (request.lines.isEmpty()) throw DatabaseError("a sale requires at least one item");
    Transaction tx(db_->handle());
    Money subtotal{};
    for (const auto& line: request.lines) {
        if(line.quantity<=0 || line.unitPrice<0 || line.discount<0) throw DatabaseError("invalid sale line");
        const Money lineTotal=line.quantity*line.unitPrice-line.discount;
        if (lineTotal<0) throw DatabaseError("line discount exceeds line amount");
        subtotal+=lineTotal;
    }
    if (request.invoiceDiscount<0 || request.invoiceDiscount>subtotal) throw DatabaseError("invalid invoice discount");
    const Money total=subtotal-request.invoiceDiscount;
    if(request.paidAmount<0 || request.paidAmount>total) throw DatabaseError("invalid payment amount");
    const auto due=total-request.paidAmount;
    const QStringList paymentMethods{"cash","credit","cheque","mobile_wallet","mixed"};
    if (!paymentMethods.contains(request.paymentMethod)) throw DatabaseError("unsupported payment method");
    if (due>0 && request.customerId.isEmpty()) throw DatabaseError("an unpaid balance requires a customer");
    if (!request.customerId.isEmpty() && due>0) {
        auto c=db_->prepare("SELECT balance_paisa, credit_limit_paisa FROM customers WHERE id=? AND is_deleted=0"); c.bind(1,request.customerId);
        if(!c.stepRow()) throw DatabaseError("customer not found");
        if(c.integer(1)>0 && c.integer(0)+due>c.integer(1)) throw DatabaseError("customer credit limit exceeded");
    }
    const QString id=uuid(); const QString invoice=QString("INV-%1-%2").arg(QDate::currentDate().toString("yyyyMMdd"), id.left(6).toUpper());
    auto sale=db_->prepare("INSERT INTO sales(id,invoice_no,customer_id,shift_id,status,payment_method,subtotal_paisa,discount_paisa,total_paisa,paid_paisa,due_paisa,note,created_at) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)");
    sale.bind(1,id); sale.bind(2,invoice); if(request.customerId.isEmpty()) sale.bindNull(3); else sale.bind(3,request.customerId); if(request.shiftId.isEmpty()) sale.bindNull(4); else sale.bind(4,request.shiftId); sale.bind(5,"completed"); sale.bind(6,request.paymentMethod); sale.bind(7,subtotal); sale.bind(8,request.invoiceDiscount); sale.bind(9,total); sale.bind(10,request.paidAmount); sale.bind(11,due); sale.bind(12,request.note); sale.bind(13,utcNow()); sale.execute();
    for (const auto& line: request.lines) {
        const auto allocations=allocateBatches(line,id,"POS");
        Money discountRemaining=line.discount;
        for (int index=0; index<allocations.size(); ++index) {
            const auto& allocation=allocations[index];
            const Money allocationDiscount=index==allocations.size()-1 ? discountRemaining : (line.discount*allocation.quantity)/line.quantity;
            discountRemaining-=allocationDiscount;
            const Money allocationTotal=allocation.quantity*line.unitPrice-allocationDiscount;
            auto item=db_->prepare("INSERT INTO sale_items(id,sale_id,product_id,batch_id,quantity,unit_name,unit_price_paisa,discount_paisa,line_total_paisa) VALUES(?,?,?,?,?,?,?,?,?)");
            item.bind(1,uuid()); item.bind(2,id); item.bind(3,line.productId); if(allocation.batchId.isEmpty()) item.bindNull(4); else item.bind(4,allocation.batchId); item.bind(5,allocation.quantity); item.bind(6,line.unitName); item.bind(7,line.unitPrice); item.bind(8,allocationDiscount); item.bind(9,allocationTotal); item.execute();
        }
    }
    if (!request.customerId.isEmpty() && due>0) {
        auto balance=db_->prepare("UPDATE customers SET balance_paisa=balance_paisa+? WHERE id=?"); balance.bind(1,due); balance.bind(2,request.customerId); balance.execute();
        auto ledger=db_->prepare("INSERT INTO customer_ledger(id,customer_id,sale_id,description,debit_paisa,credit_paisa,running_balance_paisa,created_at) SELECT ?,?,?,'Credit sale',?,0,balance_paisa,? FROM customers WHERE id=?"); ledger.bind(1,uuid()); ledger.bind(2,request.customerId); ledger.bind(3,id); ledger.bind(4,due); ledger.bind(5,utcNow()); ledger.bind(6,request.customerId); ledger.execute();
    }
    if (request.paidAmount>0 && request.paymentMethod=="cash") { auto cash=db_->prepare("INSERT INTO cash_transactions(id,shift_id,sale_id,type,amount_paisa,reason,created_at) VALUES(?,?,?,?,?,?,?)"); cash.bind(1,uuid()); if(request.shiftId.isEmpty()) cash.bindNull(2); else cash.bind(2,request.shiftId); cash.bind(3,id); cash.bind(4,"cash_in"); cash.bind(5,request.paidAmount); cash.bind(6,"Sale payment"); cash.bind(7,utcNow()); cash.execute(); }
    tx.commit(); return {id,invoice,total};
}

void PosService::voidSale(const QString& saleId, const QString& reason, const QString& performedBy) {
    if(reason.trimmed().isEmpty()) throw DatabaseError("a void reason is required");
    Transaction tx(db_->handle()); auto sale=db_->prepare("SELECT customer_id,due_paisa,paid_paisa,payment_method,status FROM sales WHERE id=?"); sale.bind(1,saleId); if(!sale.stepRow()) throw DatabaseError("sale not found"); const auto customer=sale.text(0); const auto due=sale.integer(1); const auto paid=sale.integer(2); const auto paymentMethod=sale.text(3); if(sale.text(4)!="completed") throw DatabaseError("only completed, unpaid sales can be voided");
    auto allocations=db_->prepare("SELECT 1 FROM customer_payment_allocations WHERE sale_id=? LIMIT 1"); allocations.bind(1,saleId); if(allocations.stepRow()) throw DatabaseError("a sale with recorded customer payments must be refunded, not voided");
    auto lines=db_->prepare("SELECT product_id,batch_id,quantity FROM sale_items WHERE sale_id=?"); lines.bind(1,saleId);
    while(lines.stepRow()) { const auto product=lines.text(0), batch=lines.text(1); const auto quantity=lines.integer(2); auto p=db_->prepare("UPDATE products SET stock_quantity=stock_quantity+?,updated_at=? WHERE id=?"); p.bind(1,quantity); p.bind(2,utcNow()); p.bind(3,product); p.execute(); if(!batch.isEmpty()){auto b=db_->prepare("UPDATE batches SET quantity_remaining=quantity_remaining+? WHERE id=?");b.bind(1,quantity);b.bind(2,batch);b.execute();} auto m=db_->prepare("INSERT INTO stock_movements(id,product_id,batch_id,type,quantity,reference_id,reason,balance_after,performed_by,created_at) SELECT ?,?,?, 'void_return', ?, ?, ?, stock_quantity, ?, ? FROM products WHERE id=?");m.bind(1,uuid());m.bind(2,product);if(batch.isEmpty())m.bindNull(3);else m.bind(3,batch);m.bind(4,quantity);m.bind(5,saleId);m.bind(6,reason);m.bind(7,performedBy);m.bind(8,utcNow());m.bind(9,product);m.execute(); }
    auto mark=db_->prepare("UPDATE sales SET status='voided',voided_at=?,void_reason=? WHERE id=?"); mark.bind(1,utcNow());mark.bind(2,reason);mark.bind(3,saleId);mark.execute();
    if(!customer.isEmpty() && due>0){auto c=db_->prepare("UPDATE customers SET balance_paisa=balance_paisa-? WHERE id=?");c.bind(1,due);c.bind(2,customer);c.execute();auto ledger=db_->prepare("INSERT INTO customer_ledger(id,customer_id,sale_id,description,debit_paisa,credit_paisa,running_balance_paisa,created_at) SELECT ?,?,?,'Voided sale reversal',0,?,balance_paisa,? FROM customers WHERE id=?");ledger.bind(1,uuid());ledger.bind(2,customer);ledger.bind(3,saleId);ledger.bind(4,due);ledger.bind(5,utcNow());ledger.bind(6,customer);ledger.execute();}
    if (paid>0 && paymentMethod=="cash") { auto cash=db_->prepare("INSERT INTO cash_transactions(id,shift_id,sale_id,type,amount_paisa,reason,created_at) SELECT ?,shift_id,?,'cash_out',?,'Voided sale reversal',? FROM sales WHERE id=?");cash.bind(1,uuid());cash.bind(2,saleId);cash.bind(3,paid);cash.bind(4,utcNow());cash.bind(5,saleId);cash.execute(); }
    auto audit=db_->prepare("INSERT INTO audit_log(id,action,entity_type,entity_id,detail,created_at) VALUES(?,?,?,?,?,?)");audit.bind(1,uuid());audit.bind(2,"sale_voided");audit.bind(3,"sale");audit.bind(4,saleId);audit.bind(5,reason);audit.bind(6,utcNow());audit.execute();tx.commit();
}
} // namespace pos
