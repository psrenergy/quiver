#include "database_impl.h"

namespace quiver {

void Database::delete_element(const std::string& collection, int64_t id) {
    impl_->logger->debug("Deleting element {} from collection: {}", id, collection);
    impl_->require_collection(collection, "delete_element");

    if (execute("SELECT 1 FROM " + collection + " WHERE id = ?", {id}).empty()) {
        throw std::runtime_error("Element not found: " + std::to_string(id) + " in collection '" + collection + "'");
    }

    auto sql = "DELETE FROM " + collection + " WHERE id = ?";
    execute(sql, {id});

    impl_->logger->info("Deleted element {} from {}", id, collection);
}

}  // namespace quiver
