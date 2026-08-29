#include "core/data_change_bus.h"

namespace pos {

DataChangeBus::DataChangeBus(QObject* parent) : QObject(parent) {}

DataChangeBus& DataChangeBus::instance() {
    static DataChangeBus bus;
    return bus;
}

void notifyInventoryChanged() { emit DataChangeBus::instance().inventoryChanged(); }
void notifySalesChanged() { emit DataChangeBus::instance().salesChanged(); }
void notifyPurchasesChanged() { emit DataChangeBus::instance().purchasesChanged(); }
void notifyCustomersChanged() { emit DataChangeBus::instance().customersChanged(); }
void notifySuppliersChanged() { emit DataChangeBus::instance().suppliersChanged(); }
void notifyCashChanged() { emit DataChangeBus::instance().cashChanged(); }

} // namespace pos
