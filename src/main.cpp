#include "core/database.h"
#include "core/seed_service.h"
#include "ui/main_window.h"
#include <QApplication>
#include <QMessageBox>
#include <QStandardPaths>
#include <filesystem>
#include <memory>

int main(int argc,char* argv[]) {
    QApplication app(argc,argv); QApplication::setApplicationName("Nexora POS"); QApplication::setOrganizationName("Nexora");
    try { const auto data=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation); std::filesystem::create_directories(data.toStdWString()); auto db=std::make_shared<pos::Database>(std::filesystem::path(data.toStdWString())/L"business.db"); db->migrate(); QStringList missing; if(!db->isSchemaCompatible(&missing)){QMessageBox::critical(nullptr,"Database upgrade needed",QString("The database schema is incomplete:\n%1").arg(missing.join("\n")));return 2;} if(!db->quickCheck()){QMessageBox::critical(nullptr,"Database recovery needed","The local database did not pass its integrity check. Restore the most recent verified backup before taking sales.");return 2;} if(QCoreApplication::arguments().contains("--seed-demo")){pos::SeedService(db).seedDemoData(); return 0;} MainWindow window(db);window.show();return app.exec(); }
    catch(const std::exception& error){QMessageBox::critical(nullptr,"Unable to start Nexora POS",error.what());return 1;}
}
