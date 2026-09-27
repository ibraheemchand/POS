#include "core/app_paths.h"
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>

namespace pos::paths {

QString appDataDir() {
    QString base = qEnvironmentVariable("PROGRAMDATA");
    if (base.trimmed().isEmpty())
        base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString dir = QDir(base).filePath("Invento");
    QDir().mkpath(dir);
    return QDir::toNativeSeparators(dir);
}

QString databaseFilePath() { return QDir(appDataDir()).filePath("business.db"); }

QString backupsDir() {
    const QString dir = QDir(appDataDir()).filePath("backups");
    QDir().mkpath(dir);
    return dir;
}

QString logsDir() {
    const QString dir = QDir(appDataDir()).filePath("logs");
    QDir().mkpath(dir);
    return dir;
}

QString receiptsDir() {
    const QString dir = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath("Invento Receipts");
    QDir().mkpath(dir);
    return dir;
}

QString legacyAppDataDir() {
    QString base = qEnvironmentVariable("APPDATA");
    if (base.trimmed().isEmpty()) return {};
    return QDir(base).filePath("Invento/Invento");
}

QString legacyExeDir() { return QCoreApplication::applicationDirPath(); }

} // namespace pos::paths
