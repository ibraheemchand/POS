#include "core/barcode_service.h"
#include "core/database.h"
#include <QPdfWriter>
#include <QPainter>
#include <QPageSize>
#include <QRegularExpression>

namespace pos {
QString BarcodeService::generateEan13(const QString& twelveDigits) const {if(twelveDigits.size()!=12||!twelveDigits.contains(QRegularExpression("^\\d{12}$")))throw DatabaseError("EAN-13 requires exactly twelve digits");int sum{};for(int i=0;i<12;++i)sum+=(twelveDigits[i].digitValue()*(i%2?3:1));return twelveDigits+QString::number((10-sum%10)%10);}
bool BarcodeService::isValidEan13(const QString& barcode) const {if(barcode.size()!=13||!barcode.contains(QRegularExpression("^\\d{13}$")))return false;return generateEan13(barcode.left(12))==barcode;}
void BarcodeService::printLabelsPdf(const QList<BarcodeLabel>& labels,const QString& destination) const {if(labels.isEmpty()||destination.trimmed().isEmpty())throw DatabaseError("labels and a destination are required");QPdfWriter writer(destination);writer.setPageSize(QPageSize(QPageSize::A4));writer.setResolution(300);QPainter painter(&writer);if(!painter.isActive())throw DatabaseError("cannot create barcode PDF");int y=150;for(const auto& label:labels){if(!isValidEan13(label.barcode))throw DatabaseError("label has invalid EAN-13 barcode");painter.setFont(QFont("Arial",12,QFont::Bold));painter.drawText(100,y,label.productName);painter.setFont(QFont("Courier New",14));painter.drawText(100,y+70,QString("|||| %1 ||||").arg(label.barcode));painter.setFont(QFont("Arial",9));painter.drawText(100,y+105,label.unitName);y+=160;if(y>3200){writer.newPage();y=150;}}}
} // namespace pos
