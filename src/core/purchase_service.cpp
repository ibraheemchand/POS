#include "core/purchase_service.h"
#include "core/database.h"
#include "core/shift_service.h"
#include <QStringList>

namespace pos {
PurchaseService::PurchaseService(std::shared_ptr<Database> database) : db_(std::move(database)) {}

PurchaseResult PurchaseService::completePurchase(const PurchaseRequest& request) {
    if (request.supplierId.isEmpty() || request.lines.isEmpty()) throw DatabaseError("a purchase requires a supplier and at least one item");
    const QStringList methods{"cash","credit","cheque","mobile_wallet","mixed"};
    if (!methods.contains(request.paymentMethod)) throw DatabaseError("unsupported purchase payment method");
    Money subtotal{};
    for (const auto& line : request.lines) {
        if (line.productId.isEmpty() || line.quantity<=0 || line.unitPrice<0 || line.discount<0 || line.tax<0) throw DatabaseError("invalid purchase line");
        const Money gross=line.quantity*line.unitPrice;
        if (line.discount>gross) throw DatabaseError("purchase line discount exceeds its amount");
        subtotal+=gross-line.discount+line.tax;
    }
    if (request.invoiceDiscount<0 || request.invoiceDiscount>subtotal || request.invoiceTax<0) throw DatabaseError("invalid purchase totals");
    const Money total=subtotal-request.invoiceDiscount+request.invoiceTax;
    if (request.paidAmount<0 || request.paidAmount>total) throw DatabaseError("invalid purchase payment");

    if(request.paidAmount>0 && request.paymentMethod=="cash" && request.shiftId.isEmpty()) ensureActiveShift(*db_);
    Transaction tx(db_->handle());
    QString cashShift=request.shiftId;
    if(request.paidAmount>0 && request.paymentMethod=="cash") { if(cashShift.isEmpty()) cashShift=activeShiftId(*db_); if(cashShift.isEmpty()) throw DatabaseError("open a cashier shift before recording cash purchases"); }
    auto supplier=db_->prepare("SELECT balance_paisa FROM suppliers WHERE id=? AND is_archived=0"); supplier.bind(1,request.supplierId);
    if (!supplier.stepRow()) throw DatabaseError("supplier not found or archived");
    const auto id=uuid();
    const auto invoice=QString("PUR-%1-%2").arg(QDate::currentDate().toString("yyyyMMdd"),id.left(6).toUpper());
    const Money due=total-request.paidAmount;
    auto purchase=db_->prepare("INSERT INTO purchases(id,invoice_no,supplier_id,warehouse_id,status,subtotal_paisa,discount_paisa,tax_paisa,total_paisa,paid_paisa,due_paisa,purchased_at,notes) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)");
    purchase.bind(1,id); purchase.bind(2,invoice); purchase.bind(3,request.supplierId); if(request.warehouseId.isEmpty()) purchase.bindNull(4); else purchase.bind(4,request.warehouseId); purchase.bind(5,"completed"); purchase.bind(6,subtotal); purchase.bind(7,request.invoiceDiscount); purchase.bind(8,request.invoiceTax); purchase.bind(9,total); purchase.bind(10,request.paidAmount); purchase.bind(11,due); purchase.bind(12,utcNow()); purchase.bind(13,request.note); purchase.execute();
    for (const auto& line : request.lines) {
        auto product=db_->prepare("SELECT stock_quantity,track_batches FROM products WHERE id=? AND is_deleted=0"); product.bind(1,line.productId);
        if(!product.stepRow()) throw DatabaseError("purchase product not found or archived");
        const auto before=product.integer(0); const bool tracks=product.integer(1)!=0; QString batchId;
        if(tracks) {
            if(line.batchNo.trimmed().isEmpty()) throw DatabaseError("a batch number is required for batch-tracked products");
            batchId=uuid(); auto batch=db_->prepare("INSERT INTO batches(id,product_id,batch_no,expiry_date,quantity_remaining,purchase_price_paisa,created_at) VALUES(?,?,?,?,?,?,?)");
            batch.bind(1,batchId); batch.bind(2,line.productId); batch.bind(3,line.batchNo.trimmed()); if(line.expiryDate.isValid()) batch.bind(4,line.expiryDate.toString(Qt::ISODate)); else batch.bindNull(4); batch.bind(5,line.quantity); batch.bind(6,line.unitPrice); batch.bind(7,utcNow()); batch.execute();
        }
        auto productUpdate=db_->prepare("UPDATE products SET stock_quantity=stock_quantity+?,purchase_price_paisa=?,updated_at=? WHERE id=?"); productUpdate.bind(1,line.quantity); productUpdate.bind(2,line.unitPrice); productUpdate.bind(3,utcNow()); productUpdate.bind(4,line.productId); productUpdate.execute();
        const Money lineTotal=line.quantity*line.unitPrice-line.discount+line.tax;
        auto item=db_->prepare("INSERT INTO purchase_items(id,purchase_id,product_id,batch_id,quantity,unit_name,unit_price_paisa,discount_paisa,tax_paisa,line_total_paisa) VALUES(?,?,?,?,?,?,?,?,?,?)"); item.bind(1,uuid()); item.bind(2,id); item.bind(3,line.productId); if(batchId.isEmpty()) item.bindNull(4); else item.bind(4,batchId); item.bind(5,line.quantity); item.bind(6,line.unitName); item.bind(7,line.unitPrice); item.bind(8,line.discount); item.bind(9,line.tax); item.bind(10,lineTotal); item.execute();
        auto movement=db_->prepare("INSERT INTO stock_movements(id,product_id,batch_id,type,quantity,original_unit,reference_id,reason,balance_after,performed_by,created_at) VALUES(?,?,?,?,?,?,?,?,?,?,?)"); movement.bind(1,uuid()); movement.bind(2,line.productId); if(batchId.isEmpty()) movement.bindNull(3); else movement.bind(3,batchId); movement.bind(4,"purchase"); movement.bind(5,line.quantity); movement.bind(6,line.unitName); movement.bind(7,id); movement.bind(8,"Purchase received"); movement.bind(9,before+line.quantity); movement.bind(10,"Purchasing"); movement.bind(11,utcNow()); movement.execute();
    }
    if(due>0) { auto balance=db_->prepare("UPDATE suppliers SET balance_paisa=balance_paisa+? WHERE id=?"); balance.bind(1,due); balance.bind(2,request.supplierId); balance.execute(); auto ledger=db_->prepare("INSERT INTO supplier_ledger(id,supplier_id,reference_id,entry_type,debit_paisa,credit_paisa,balance_paisa,created_at) SELECT ?,?,?, 'purchase', ?,0,balance_paisa,? FROM suppliers WHERE id=?"); ledger.bind(1,uuid()); ledger.bind(2,request.supplierId); ledger.bind(3,id); ledger.bind(4,due); ledger.bind(5,utcNow()); ledger.bind(6,request.supplierId); ledger.execute(); }
    if(request.paidAmount>0 && request.paymentMethod=="cash") { auto cash=db_->prepare("INSERT INTO cash_transactions(id,shift_id,sale_id,type,amount_paisa,reason,created_at) VALUES(?,?,?, 'cash_out',?,?,?)"); cash.bind(1,uuid()); cash.bind(2,cashShift); cash.bindNull(3); cash.bind(4,request.paidAmount); cash.bind(5,"Purchase payment"); cash.bind(6,utcNow()); cash.execute(); }
    tx.commit(); return {id,invoice,total};
}
} // namespace pos
