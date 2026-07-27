#include "core/thermal_print_service.h"
#include "core/database.h"
#include <QFile>

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
        result.append(textLine(QString("%1 x%2  %3").arg(item.name.trimmed()).arg(item.quantity).arg(item.lineTotal)));
    }
    result.append(textLine("--------------------------------"));
    result.append(textLine(QString("TOTAL (paisa): %1").arg(total)));
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
    if (devicePath.trimmed().isEmpty() || bytes.isEmpty()) throw DatabaseError("thermal printer path and data are required");
    QFile device(devicePath.trimmed());
    if (!device.open(QIODevice::WriteOnly)) throw DatabaseError(QString("could not open thermal printer: %1").arg(device.errorString()));
    if (device.write(bytes) != bytes.size()) throw DatabaseError("thermal printer write was incomplete");
    device.flush();
}

} // namespace pos
