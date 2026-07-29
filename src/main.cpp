#include "core/database.h"
#include "core/seed_service.h"
#include "ui/main_window.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDebug>
#include <filesystem>
#include <memory>

namespace {

std::shared_ptr<pos::Database> openDatabase(const QString& overrideDataPath = {}) {
    const auto data = overrideDataPath.trimmed().isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
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
    const auto dataDirectory = dataDirectoryArgument(argc, argv);
    if (hasSeedArgument(argc, argv)) {
        QCoreApplication app(argc, argv);
        QCoreApplication::setApplicationName("Nexora POS");
        QCoreApplication::setOrganizationName("Nexora");
        try {
            return runSeedCommand(app.arguments());
        } catch (const std::exception& error) {
            qCritical() << "Seed command failed:" << error.what();
            return 1;
        }
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName("Nexora POS");
    QApplication::setOrganizationName("Nexora");
    try {
        auto database = openDatabase(dataDirectory);
        MainWindow window(database);
        window.show();
        return app.exec();
    } catch (const std::exception& error) {
        QMessageBox::critical(nullptr, "Unable to start Nexora POS", error.what());
        return 1;
    }
}
