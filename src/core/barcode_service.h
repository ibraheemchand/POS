#pragma once
#include <QString>
#include <QList>
namespace pos {
struct BarcodeLabel { QString productName; QString barcode; QString unitName; };
class BarcodeService {
public:
    QString generateEan13(const QString& twelveDigits) const;
    bool isValidEan13(const QString& barcode) const;
    void printLabelsPdf(const QList<BarcodeLabel>& labels, const QString& destination) const;
};
} // namespace pos
