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

void printViaDriver(const ReceiptData& data, const QString& printerName, double paperMm) {
    const auto info = QPrinterInfo::printerInfo(printerName);
    if (info.isNull())
        throw DatabaseError(QString("Printer \"%1\" was not found. Pick an installed printer in Settings.").arg(printerName));
    QPrinter printer(info, QPrinter::HighResolution);
    QPainter painter(&printer);
    if (!painter.isActive())
        throw DatabaseError(QString("Could not start a print job on \"%1\".").arg(printerName));
    // Render the same table at the paper's printable width (mm) into the top-left.
    const double printableMm = paperMm < 60 ? 48.0 : 72.0;
    const double pxPerMm = printer.resolution() / 25.4;
    ReceiptService::paint(painter, printableMm * pxPerMm, data, printableMm);
    painter.end();
}

QString deliverReceipt(std::shared_ptr<Database> db, const ReceiptData& data) {
    SettingsService settings(db);
    const auto mode = settings.value("printer.mode", "raw");
    const auto path = settings.value("printer.thermal_path").trimmed();
    const double paperMm = settings.value("printer.paper_mm", "80").toDouble();
    if (path.isEmpty())
        throw DatabaseError("No printer is configured. Open Settings, choose a printer and mode, then try again.");
    if (mode == "normal") {
        printViaDriver(data, path, paperMm);
        return QString("Printed via the Windows driver (normal mode) to \"%1\".").arg(path);
    }
    // Raw thermal: print the layout as a bitmap so the table/grid matches the PDF.
    ThermalPrintService::writeRaw(path, ReceiptService::escposRasterBytes(data, paperMm));
    return QString("Sent raster receipt (thermal mode, %1mm) to \"%2\".").arg(int(paperMm)).arg(path);
}

} // namespace pos::ui
