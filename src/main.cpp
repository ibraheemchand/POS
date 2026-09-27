#include "core/database.h"
#include "core/seed_service.h"
#include "core/security_service.h"
#include "core/settings_service.h"
#include "core/app_paths.h"
#include "core/data_migration.h"
#include "core/logger.h"
#include "ui/main_window.h"
#include "ui/first_run_wizard.h"
#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QCoreApplication>
#include <QDateTime>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDebug>
#include <filesystem>
#include <memory>

namespace {

std::shared_ptr<pos::Database> openDatabase(const QString& overrideDataPath = {}) {
    const auto data = overrideDataPath.trimmed().isEmpty()
        ? pos::paths::appDataDir()
        : overrideDataPath.trimmed();
    std::filesystem::create_directories(data.toStdWString());
    auto database = std::make_shared<pos::Database>(std::filesystem::path(data.toStdWString()) / L"business.db");
    database->migrate();
    QStringList missing;
    if (!database->isSchemaCompatible(&missing)) {
        throw pos::DatabaseError(QString("The database schema is incomplete: %1").arg(missing.join(", ")).toStdString());
    }
    if (!database->quickCheck()) {
        throw pos::DatabaseError("The local database failed its integrity check");
    }
    return database;
}

bool hasSeedArgument(int argc, char* argv[]) {
    for (int index = 1; index < argc; ++index) {
        const auto argument = QString::fromLocal8Bit(argv[index]);
        if (argument == "--seed-demo" || argument.startsWith("--seed-random=")) return true;
    }
    return false;
}

QString dataDirectoryArgument(int argc, char* argv[]) {
    for (int index = 1; index < argc; ++index) {
        const auto argument = QString::fromLocal8Bit(argv[index]);
        if (argument.startsWith("--data-dir=")) return argument.mid(QString("--data-dir=").size());
    }
    return {};
}

int runSeedCommand(const QStringList& arguments) {
    QString dataPath;
    for (const auto& argument : arguments) {
        if (argument.startsWith("--data-dir=")) dataPath = argument.mid(QString("--data-dir=").size());
    }
    auto database = openDatabase(dataPath);
    if (arguments.contains("--seed-demo")) {
        pos::SeedService(database).seedDemoData();
        return 0;
    }
    for (const auto& argument : arguments) {
        if (!argument.startsWith("--seed-random=")) continue;
        bool countOk = false;
        const auto count = argument.mid(QString("--seed-random=").size()).toInt(&countOk);
        if (!countOk) throw pos::DatabaseError("--seed-random requires a numeric count");
        quint32 seed = static_cast<quint32>(QDateTime::currentSecsSinceEpoch());
        for (const auto& candidate : arguments) {
            if (!candidate.startsWith("--seed=")) continue;
            bool seedOk = false;
            const auto parsed = candidate.mid(QString("--seed=").size()).toUInt(&seedOk);
            if (!seedOk) throw pos::DatabaseError("--seed requires a numeric value");
            seed = parsed;
        }
        pos::SeedService(database).seedRandomData(count, seed);
        return 0;
    }
    throw pos::DatabaseError("unknown seed command");
}

} // namespace

int main(int argc, char* argv[]) {
    // Standard Qt6 high-DPI: honor the OS per-monitor scaling and render fractional
    // factors (125/150%) precisely. No manual scale factors — 150% on a laptop and
    // 100% on a monitor therefore give the same physical text size. Must be set
    // before any Q(Gui)Application exists.
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    const auto dataDirectory = dataDirectoryArgument(argc, argv);
    if (hasSeedArgument(argc, argv)) {
        QCoreApplication app(argc, argv);
        QCoreApplication::setApplicationName("Invento");
        QCoreApplication::setOrganizationName("Invento");
        try {
            return runSeedCommand(app.arguments());
        } catch (const std::exception& error) {
            qCritical() << "Seed command failed:" << error.what();
            return 1;
        }
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName("Invento");
    QApplication::setOrganizationName("Invento");
    QApplication::setWindowIcon(QIcon(":/branding/app_icon"));
    try {
        pos::Logger::instance().configure(std::filesystem::path(pos::paths::logsDir().toStdWString()));
        POS_LOG_INFO("Invento starting");

        // One-time copy of an old "Invento" database into the shared location. The old
        // file is copied (never moved) and verified before use; on any mismatch the app
        // keeps using the old database and reports it.
        QString dbDir = dataDirectory;
        if (dbDir.trimmed().isEmpty()) {
            const auto migration = pos::migrateLegacyDatabaseIfNeeded();
            dbDir = migration.databaseDir;
            if (migration.status == pos::MigrationOutcome::Migrated)
                QMessageBox::information(nullptr, "Database moved to a shared location", migration.message);
            else if (migration.isError)
                QMessageBox::critical(nullptr, "Database migration could not be verified", migration.message);
        }

        auto database = openDatabase(dbDir);
        // Point-based base font (physical-size units) times the user's "UI size"
        // preference (80/90/100/110%, default 100). High-DPI handles the rest.
        const int uiSize = qBound(80, pos::SettingsService(database).value("ui.size_percent", "100").toInt(), 110);
        { QFont base = app.font(); base.setPointSizeF(10.5 * uiSize / 100.0); app.setFont(base); }

        // Fresh database (no owner PIN yet) → first-run setup wizard. It is the only
        // path that can load sample data, and only if the user opts in.
        if (!pos::SecurityService(database).hasPin()) {
            FirstRunWizard wizard(database);
            if (wizard.exec() != QDialog::Accepted) return 0; // setup is mandatory to run
        }

        MainWindow window(database);
        window.showMaximized();
        return app.exec();
    } catch (const std::exception& error) {
        POS_LOG_CRITICAL(QString("Startup failed: %1").arg(error.what()));
        QMessageBox::critical(nullptr, "Unable to start Invento", error.what());
        return 1;
    }
}
