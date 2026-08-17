#include "database_impl.h"

namespace quiver {

void Database::delete_element(const std::string& collection, int64_t id) {
    impl_->logger->debug("Deleting element {} from collection: {}", id, collection);
    impl_->require_collection(collection, "delete_element");

    impl_->require_element(collection, id, *this);

    auto sql = "DELETE FROM " + collection + " WHERE id = ?";
    execute(sql, {id});

    impl_->logger->info("Deleted element {} from {}", id, collection);
}

void Database::delete_element_by_label(const std::string& collection, const std::string& label) {
    delete_element(collection, impl_->resolve_label(collection, label, "delete_element_by_label", *this));
}

}  // namespace quiver
