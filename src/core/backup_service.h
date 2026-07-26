#pragma once
#include <filesystem>
#include <memory>
#include <QString>
#include <QList>
namespace pos {
class Database;
struct BackupInfo { QString id; std::filesystem::path file; QString checksum; QString verifiedAt; QString status; };
class BackupService {
public:
    explicit BackupService(std::shared_ptr<Database> database);
    void createVerifiedBackup(const std::filesystem::path& file);
    bool verifyBackup(const std::filesystem::path& file) const;
    QList<BackupInfo> verifiedBackups() const;
    void restoreVerifiedBackup(const std::filesystem::path& file, const std::filesystem::path& safetyCopy);
private:
    std::shared_ptr<Database> db_;
};
}
