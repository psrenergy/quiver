#include "database_impl.h"

namespace quiver {

void Database::delete_element_by_id(const std::string& collection, int64_t id) {
    impl_->logger->debug("Deleting element {} from collection: {}", id, collection);
    impl_->require_collection(collection, "delete element");

    auto sql = "DELETE FROM " + collection + " WHERE id = ?";
    execute(sql, {id});

    impl_->logger->info("Deleted element {} from {}", id, collection);
}

}  // namespace quiver
