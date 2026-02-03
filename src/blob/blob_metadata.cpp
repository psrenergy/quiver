#include "quiver/blob/blob_metadata.h"
#include "quiver/blob/dimension.h"

#include <cstdint>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

namespace {

} // anonymous namespace

namespace quiver {

struct BlobMetadata::Impl {
    std::vector<std::unique_ptr<Dimension>> dimensions;
    std::time_t initial_datetime;
    std::string unit;
    std::vector<std::string> labels;
    std::string version;
    //
    int64_t number_of_time_dimensions;

};

} // namespace quiver