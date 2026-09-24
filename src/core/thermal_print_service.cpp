#include "core/thermal_print_service.h"
#include "core/database.h"
#include "core/types.h"
#include <QFile>

#ifdef _WIN32
#include <windows.h>
#include <winspool.h>
namespace {
// Sends raw ESC/POS bytes to an installed Windows printer BY NAME via the print
// spooler. This is the correct way — opening a printer as a file (the old code)
// gives "Access is denied" because printers are not files.
void writeRawToWindowsPrinter(const QString& printerName, const QByteArray& bytes) {
    std::wstring name = printerName.toStdWString();
    HANDLE handle = nullptr;
    if (!OpenPrinterW(name.data(), &handle, nullptr))
        throw pos::DatabaseError(QString("Windows could not open printer \"%1\" (error %2). Check that the printer is installed and shared/allowed for this user, or pick another one in Settings.")
                                     .arg(printerName).arg(static_cast<qulonglong>(GetLastError())));
    std::wstring docName = L"POS Receipt", dataType = L"RAW";
    DOC_INFO_1W info{};
    info.pDocName = docName.data();
    info.pOutputFile = nullptr;
    info.pDatatype = dataType.data();
    bool ok = StartDocPrinterW(handle, 1, reinterpret_cast<LPBYTE>(&info)) != 0;
    if (ok) {
        ok = StartPagePrinter(handle) != 0;
        DWORD written = 0;
        ok = ok && WritePrinter(handle, const_cast<char*>(bytes.constData()), static_cast<DWORD>(bytes.size()), &written)
                && written == static_cast<DWORD>(bytes.size());
        EndPagePrinter(handle);
        EndDocPrinter(handle);
    }
    const auto err = GetLastError();
    ClosePrinter(handle);
    if (!ok) throw pos::DatabaseError(QString("The print job to \"%1\" did not complete (error %2).").arg(printerName).arg(static_cast<qulonglong>(err)));
}
// A plain installed-printer name vs. a device path (COM/LPT port, UNC share, file).
bool looksLikeDevicePath(const QString& p) {
    return p.startsWith("\\\\") || p.contains('/') || p.contains(':')
        || p.startsWith("COM", Qt::CaseInsensitive) || p.startsWith("LPT", Qt::CaseInsensitive);
}
} // namespace
#endif

namespace pos {
namespace {
constexpr char Esc = '\x1b';
constexpr char Gs = '\x1d';

QByteArray textLine(const QString& text) {
    QByteArray result = text.toUtf8();
    result.append('\n');
    return result;
}

void validateBarcode(const QString& barcode) {
    const auto value = barcode.trimmed();
    if (value.isEmpty() || value.size() > 64) throw DatabaseError("barcode must contain 1 to 64 characters");
    for (const auto ch : value) {
        if (ch.unicode() < 0x20 || ch.unicode() > 0x7e) throw DatabaseError("barcode must contain printable ASCII characters");
    }
}

void validateText(const QString& text, const char* field) {
    if (text.trimmed().isEmpty()) throw DatabaseError(QString("%1 is required").arg(field));
    for (const auto ch : text) {
        if (ch.unicode() < 0x20 || ch.unicode() == 0x7f) throw DatabaseError(QString("%1 contains printer control characters").arg(field));
    }
}
}

QByteArray ThermalPrintService::receiptBytes(const QString& storeName, const QString& invoiceNo,
                                             const QList<ThermalReceiptItem>& items, Money total) {
    if (items.isEmpty() || total < 0) {
        throw DatabaseError("invalid thermal receipt data");
    }
    validateText(storeName, "store name");
    validateText(invoiceNo, "invoice number");
    QByteArray result;
    result.append(Esc).append('@');
    result.append(Esc).append('a').append('\x01');
    result.append(textLine(storeName.trimmed()));
    result.append(Esc).append('a').append('\x00');
    result.append(textLine(QString("Invoice: %1").arg(invoiceNo.trimmed())));
    result.append(textLine("--------------------------------"));
    for (const auto& item : items) {
        if (item.quantity <= 0 || item.lineTotal < 0) throw DatabaseError("invalid thermal receipt item");
        validateText(item.name, "receipt item name");
        result.append(textLine(QString("%1 x%2  PKR %3").arg(item.name.trimmed()).arg(item.quantity).arg(formatMoney(item.lineTotal))));
    }
    result.append(textLine("--------------------------------"));
    result.append(textLine(QString("TOTAL: PKR %1").arg(formatMoney(total))));
    result.append('\n').append(Esc).append('d').append('\x03');
    result.append(Gs).append('V').append('\x01');
    return result;
}

QByteArray ThermalPrintService::barcodeLabelBytes(const QString& label, const QString& barcode) {
    validateBarcode(barcode);
    validateText(label, "barcode label text");
    const auto value = barcode.trimmed().toLatin1();
    if (value.size() > 255) throw DatabaseError("barcode is too long for the printer");
    QByteArray result;
    result.append(Esc).append('@');
    result.append(Esc).append('a').append('\x01');
    result.append(textLine(label.trimmed()));
    result.append(Gs).append('h').append('\x50');
    result.append(Gs).append('w').append('\x02');
    result.append(Gs).append('k').append('I').append(static_cast<char>(value.size())).append(value);
    result.append('\n').append(Esc).append('d').append('\x03');
    result.append(Gs).append('V').append('\x01');
    return result;
}

void ThermalPrintService::writeRaw(const QString& devicePath, const QByteArray& bytes) {
    const auto path = devicePath.trimmed();
    if (path.isEmpty()) throw DatabaseError("No thermal printer is configured. Open Settings, choose your printer, then use \"Print test receipt\".");
    if (bytes.isEmpty()) throw DatabaseError("there is nothing to print");
#ifdef _WIN32
    // A plain installed-printer name goes through the spooler; a real device path
    // (COM/LPT/UNC/file) is written directly.
    if (!looksLikeDevicePath(path)) { writeRawToWindowsPrinter(path, bytes); return; }
#endif
    QFile device(path);
    if (!device.open(QIODevice::WriteOnly))
        throw DatabaseError(QString("Could not open \"%1\": %2. If this is a Windows printer, enter its exact installed name (as shown in Settings) instead of a file path.")
                                .arg(path, device.errorString()));
    if (device.write(bytes) != bytes.size()) throw DatabaseError("the printer write was incomplete");
    device.flush();
}

} // namespace pos
