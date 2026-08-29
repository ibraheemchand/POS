#pragma once

#include <QObject>

namespace pos {

/// Lightweight signal bus so UI views refresh after domain writes without polling.
class DataChangeBus final : public QObject {
    Q_OBJECT
public:
    static DataChangeBus& instance();

signals:
    void inventoryChanged();
    void salesChanged();
    void purchasesChanged();
    void customersChanged();
    void suppliersChanged();
    void cashChanged();

private:
    explicit DataChangeBus(QObject* parent = nullptr);
};

void notifyInventoryChanged();
void notifySalesChanged();
void notifyPurchasesChanged();
void notifyCustomersChanged();
void notifySuppliersChanged();
void notifyCashChanged();

} // namespace pos
