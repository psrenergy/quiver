#include "database_impl.h"

#include <string>

namespace quiver {

int64_t Database::number_of_elements(const std::string& collection) const {
    impl_->require_collection(collection, "number_of_elements");
    return query_int_rows(impl_->db, "SELECT COUNT(*) FROM \"" + collection + "\"")[0][0];
}

}  // namespace quiver
