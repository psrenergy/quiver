#ifndef QUIVER_ATTRIBUTE_METADATA_H
#define QUIVER_ATTRIBUTE_METADATA_H

#include "data_type.h"
#include "export.h"

#include <optional>
#include <string>
#include <vector>

namespace quiver {

struct QUIVER_API ScalarMetadata {
    std::string name;
    DataType data_type;
    bool not_null;
    bool primary_key;
    std::optional<std::string> default_value;
    bool is_foreign_key = false;
    std::optional<std::string> references_collection;
    std::optional<std::string> references_column;
};

struct QUIVER_API GroupMetadata {
    std::string group_name;
    std::string dimension_column;               // Empty for vector/set groups; ordering column for time series
    std::vector<ScalarMetadata> value_columns;
};

}  // namespace quiver

#endif  // QUIVER_ATTRIBUTE_METADATA_H
