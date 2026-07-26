#include "core/backup_service.h"
#include "core/database.h"
#include <QCryptographicHash>
#include <QFile>
#include <sqlite3.h>

namespace pos {
BackupService::BackupService(std::shared_ptr<Database> database):db_(std::move(database)){}
void BackupService::createVerifiedBackup(const std::filesystem::path& file) {
    std::filesystem::create_directories(file.parent_path()); db_->backupTo(file);
    if(!verifyBackup(file)) throw DatabaseError("backup integrity check failed");
    QFile input(QString::fromStdWString(file.wstring())); if(!input.open(QIODevice::ReadOnly)) throw DatabaseError("cannot read backup for checksum");
    const auto hash=QCryptographicHash::hash(input.readAll(),QCryptographicHash::Sha256).toHex(); QFile sidecar(QString::fromStdWString(file.wstring())+".sha256"); if(!sidecar.open(QIODevice::WriteOnly|QIODevice::Truncate)) throw DatabaseError("cannot write backup checksum"); sidecar.write(hash); sidecar.close();
    auto insert=db_->prepare("INSERT INTO backups(id,file_path,sha256,verified_at,status,created_at) VALUES(?,?,?,?,?,?)"); insert.bind(1,uuid());insert.bind(2,QString::fromStdWString(file.wstring()));insert.bind(3,QString::fromLatin1(hash));insert.bind(4,utcNow());insert.bind(5,"verified");insert.bind(6,utcNow());insert.execute();
}
bool BackupService::verifyBackup(const std::filesystem::path& file) const {
    if (!std::filesystem::exists(file) || std::filesystem::file_size(file)==0) return false;
    sqlite3* snapshot{};
    const auto path=file.u8string();
    if (sqlite3_open_v2(reinterpret_cast<const char*>(path.c_str()),&snapshot,SQLITE_OPEN_READONLY,nullptr)!=SQLITE_OK) { if(snapshot) sqlite3_close(snapshot); return false; }
    sqlite3_stmt* statement{};
    const bool prepared=sqlite3_prepare_v2(snapshot,"PRAGMA integrity_check",-1,&statement,nullptr)==SQLITE_OK;
    const bool valid=prepared && sqlite3_step(statement)==SQLITE_ROW && QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(statement,0)))=="ok";
    if(statement) sqlite3_finalize(statement);
    sqlite3_close(snapshot);
    if(!valid) return false;
    const auto sidecar=QString::fromStdWString(file.wstring())+".sha256";
    if(!QFile::exists(sidecar)) return true; // The file is sound; new backups always create a sidecar below.
    QFile backup(QString::fromStdWString(file.wstring())); QFile checksum(sidecar);
    if(!backup.open(QIODevice::ReadOnly)||!checksum.open(QIODevice::ReadOnly)) return false;
    return QCryptographicHash::hash(backup.readAll(),QCryptographicHash::Sha256).toHex().trimmed()==checksum.readAll().trimmed();
}
QList<BackupInfo> BackupService::verifiedBackups() const {
    QList<BackupInfo> result;
    auto query=db_->prepare("SELECT id,file_path,sha256,verified_at,status FROM backups WHERE status='verified' ORDER BY created_at DESC");
    while(query.stepRow()) result.append({query.text(0),std::filesystem::path(query.text(1).toStdWString()),query.text(2),query.text(3),query.text(4)});
    return result;
}
void BackupService::restoreVerifiedBackup(const std::filesystem::path& file, const std::filesystem::path& safetyCopy) {
    if (!verifyBackup(file)) throw DatabaseError("selected backup failed integrity verification");
    createVerifiedBackup(safetyCopy);
    db_->restoreFrom(file);
    if (!db_->integrityCheck()) throw DatabaseError("restore completed but the live database did not pass integrity check");
}
} // namespace pos
