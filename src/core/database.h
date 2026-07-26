#pragma once

#include <sqlite3.h>
#include <QString>
#include <QStringList>
#include <filesystem>
#include <stdexcept>

namespace pos {

class DatabaseError final : public std::runtime_error {
public: using std::runtime_error::runtime_error;
};

class Statement {
public:
    Statement(sqlite3* db, const char* sql);
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    void bind(int index, const QString& value);
    void bind(int index, qint64 value);
    void bindNull(int index);
    bool stepRow();
    void execute();
    QString text(int column) const;
    qint64 integer(int column) const;
private: sqlite3_stmt* stmt_{};
};

class Transaction {
public:
    explicit Transaction(sqlite3* db);
    ~Transaction();
    void commit();
    Transaction(const Transaction&) = delete;
private: sqlite3* db_; bool complete_{false};
};

class Database {
public:
    explicit Database(const std::filesystem::path& path);
    ~Database();
    Database(const Database&) = delete;
    sqlite3* handle() const { return db_; }
    const std::filesystem::path& path() const { return path_; }
    void exec(const char* sql) const;
    Statement prepare(const char* sql) const;
    void migrate();
    bool quickCheck() const;
    bool integrityCheck() const;
    bool isSchemaCompatible(QStringList* missingColumns=nullptr) const;
    void backupTo(const std::filesystem::path& destination) const;
    void restoreFrom(const std::filesystem::path& source);
private: sqlite3* db_{}; std::filesystem::path path_;
};

QString uuid();
QString utcNow();
} // namespace pos
