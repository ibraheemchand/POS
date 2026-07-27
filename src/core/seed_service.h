#pragma once

#include <memory>

namespace pos {
class Database;
class SeedService final {
public:
    explicit SeedService(std::shared_ptr<Database> database);
    void seedDemoData();
private:
    std::shared_ptr<Database> db_;
};
}
