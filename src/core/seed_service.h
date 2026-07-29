#pragma once

#include <memory>
#include <QtGlobal>

namespace pos {
class Database;
class SeedService final {
public:
    explicit SeedService(std::shared_ptr<Database> database);
    void seedDemoData();
    // Creates deterministic, idempotent QA products with integer-paisa prices.
    void seedRandomData(int productCount, quint32 seed);
private:
    std::shared_ptr<Database> db_;
};
}
