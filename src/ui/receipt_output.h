#pragma once
#include "core/receipt_service.h"
#include <QString>
#include <memory>

namespace pos { class Database; }

namespace pos::ui {

// Heuristic: printers that render pages, not raw ESC/POS (PDF/XPS/OneNote/Fax).
// Picking "raw" mode for one of these is what produced the broken receipt files.
bool isLikelyNonThermal(const QString& printerName);

// Render the receipt through the Windows print driver (QPrinter/QPainter) to the
// named printer — for Microsoft Print to PDF, A4/A5 office printers, etc. Throws.
void printViaDriver(const ReceiptData& data, const QString& printerName);

// Deliver a receipt using the configured mode. Returns a human description of what
// it did (mode + printer) for feedback. Throws on failure.
QString deliverReceipt(std::shared_ptr<Database> db, const ReceiptData& data);

} // namespace pos::ui
