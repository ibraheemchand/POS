#pragma once
#include "core/types.h"
#include <memory>
namespace pos {
class Database;
struct Customer { QString id; QString name; QString phone; Money creditLimit{}; int paymentTermsDays{}; bool archived{}; };
class CustomerService {
public:
    explicit CustomerService(std::shared_ptr<Database> database);
    QString create(const Customer& customer);
    void archive(const QString& customerId);
private: std::shared_ptr<Database> db_;
};
}
