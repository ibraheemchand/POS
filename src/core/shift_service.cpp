#include "core/shift_service.h"
#include "core/database.h"

namespace pos {
QString activeShiftId(const Database& database) {
    auto query=database.prepare("SELECT id FROM shift_sessions WHERE status='open' ORDER BY opened_at DESC LIMIT 1");
    return query.stepRow()?query.text(0):QString();
}
QString ensureActiveShift(Database& database) {
    const auto existing=activeShiftId(database); if(!existing.isEmpty()) return existing;
    Transaction tx(database.handle()); const auto id=uuid();
    auto query=database.prepare("INSERT INTO shift_sessions(id,opened_at,opening_cash_paisa,status) VALUES(?,?,0,'open')"); query.bind(1,id); query.bind(2,utcNow()); query.execute(); tx.commit(); return id;
}
ShiftService::ShiftService(std::shared_ptr<Database> db):db_(std::move(db)){}
QString ShiftService::open(Money opening) { if(opening<0) throw DatabaseError("invalid opening cash"); Transaction tx(db_->handle()); auto active=db_->prepare("SELECT 1 FROM shift_sessions WHERE status='open' LIMIT 1"); if(active.stepRow()) throw DatabaseError("a shift is already open"); const auto id=uuid(); auto q=db_->prepare("INSERT INTO shift_sessions(id,opened_at,opening_cash_paisa,status) VALUES(?,?,?,'open')"); q.bind(1,id); q.bind(2,utcNow()); q.bind(3,opening); q.execute(); tx.commit(); return id; }
ShiftClose ShiftService::close(const QString& id,Money counted) { if(counted<0) throw DatabaseError("invalid counted cash"); Transaction tx(db_->handle()); auto shift=db_->prepare("SELECT opening_cash_paisa FROM shift_sessions WHERE id=? AND status='open'"); shift.bind(1,id); if(!shift.stepRow()) throw DatabaseError("open shift not found"); const auto opening=shift.integer(0); auto net=db_->prepare("SELECT COALESCE(SUM(CASE WHEN type='cash_in' THEN amount_paisa ELSE -amount_paisa END),0) FROM cash_transactions WHERE shift_id=?"); net.bind(1,id); net.stepRow(); const auto expected=opening+net.integer(0); auto q=db_->prepare("UPDATE shift_sessions SET status='closed',closed_at=?,expected_cash_paisa=?,counted_cash_paisa=? WHERE id=?"); q.bind(1,utcNow()); q.bind(2,expected); q.bind(3,counted); q.bind(4,id); q.execute(); tx.commit(); return {expected,counted,counted-expected}; }
}
