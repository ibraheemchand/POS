#include "ui/receipt_output.h"
#include "core/database.h"
#include "core/settings_service.h"
#include "core/thermal_print_service.h"
#include <QPrinter>
#include <QPrinterInfo>
#include <QPainter>

namespace pos::ui {

bool isLikelyNonThermal(const QString& printerName) {
    static const QStringList markers = {"PDF", "XPS", "OneNote", "Fax", "Microsoft Print"};
    for (const auto& m : markers)
        if (printerName.contains(m, Qt::CaseInsensitive)) return true;
    return false;
}

void printViaDriver(const ReceiptData& data, const QString& printerName) {
    const auto info = QPrinterInfo::printerInfo(printerName);
    if (info.isNull())
        throw DatabaseError(QString("Printer \"%1\" was not found. Pick an installed printer in Settings.").arg(printerName));
    QPrinter printer(info, QPrinter::HighResolution);
    QPainter painter(&printer);
    if (!painter.isActive())
        throw DatabaseError(QString("Could not start a print job on \"%1\".").arg(printerName));
    const QRectF area(0, 0, printer.width(), printer.height());
    const QRectF margins = area.adjusted(area.width() * 0.06, area.height() * 0.05, -area.width() * 0.06, -area.height() * 0.05);
    ReceiptService::paint(painter, margins, data);
    painter.end();
}

QString deliverReceipt(std::shared_ptr<Database> db, const ReceiptData& data) {
    SettingsService settings(db);
    const auto mode = settings.value("printer.mode", "raw");
    const auto path = settings.value("printer.thermal_path").trimmed();
    if (path.isEmpty())
        throw DatabaseError("No printer is configured. Open Settings, choose a printer and mode, then try again.");
    if (mode == "normal") {
        printViaDriver(data, path);
        return QString("Printed via the Windows driver (normal mode) to \"%1\".").arg(path);
    }
    ThermalPrintService::writeRaw(path, ReceiptService::escposBytes(data));
    return QString("Sent raw ESC/POS (thermal mode) to \"%1\".").arg(path);
}

} // namespace pos::ui
