#pragma once
#include <QList>
#include <QString>
#include <QStringList>

namespace pos {
class ExcelExportService final {
public:
    static void writeWorkbook(const QString& fileName, const QStringList& headers, const QList<QStringList>& rows);
};
}
