#include "core/database.h"
#include "core/seed_service.h"
#include "core/security_service.h"
#include "ui/main_window.h"
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

// First-run owner PIN setup, shown before the main window. The PIN is required to
// run — it is never created silently by whoever opens an owner page first.
bool ensureOwnerPinConfigured(const std::shared_ptr<pos::Database>& database) {
    pos::SecurityService security(database);
    if (security.hasPin()) return true;
    while (true) {
        bool ok = false;
        const auto pin = QInputDialog::getText(nullptr, "First-time setup — Owner PIN",
            "Set an owner PIN (6-12 digits).\nIt protects commission, partner and profit data.",
            QLineEdit::Password, {}, &ok);
        if (!ok) {
            if (QMessageBox::question(nullptr, "Setup required",
                    "An owner PIN is required to run the app. Quit without setting one?") == QMessageBox::Yes)
                return false;
            continue;
        }
        const auto confirm = QInputDialog::getText(nullptr, "First-time setup — Owner PIN",
            "Re-enter the PIN:", QLineEdit::Password, {}, &ok);
        if (!ok) continue;
        if (pin != confirm) { QMessageBox::warning(nullptr, "PIN not set", "The two entries did not match."); continue; }
        try {
            const auto recovery = security.setupPin(pin);
            QMessageBox::information(nullptr, "Save your recovery code",
                "Setup complete.\n\nRecovery code (write it down — shown once; needed if you forget the PIN):\n\n"
                + recovery + "\n\nThe app is offline, so there is no email reset.");
            return true;
        } catch (const std::exception& error) {
            QMessageBox::warning(nullptr, "PIN not set", error.what());
        }
    }
}

} // namespace

int main(int argc, char* argv[]) {
    // Render fractional Windows display scaling (125%, 150%) exactly instead of
    // rounding to the nearest integer factor, which is what made 150% laptops
    // clip and overlap. Must be set before any Q(Gui)Application is constructed.
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
    // Point-based base font so text scales with the OS text-size setting and stays
    // readable (not oversized) at high DPI, instead of a fixed pixel size.
    { QFont base = app.font(); base.setPointSizeF(10.0); app.setFont(base); }
    try {
        auto database = openDatabase(dataDirectory);
        if (!ensureOwnerPinConfigured(database)) return 0; // owner cancelled mandatory PIN setup
        MainWindow window(database);
        window.showMaximized();
        return app.exec();
    } catch (const std::exception& error) {
        QMessageBox::critical(nullptr, "Unable to start Invento", error.what());
        return 1;
    }
}
