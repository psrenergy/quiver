#include "quiver/blob/blob.h"

#include <iostream>
#include <string>

namespace {
quiver::Blob& open_reader(const std::string& file_path) {}

quiver::Blob& open_writer(const std::string& file_path, const quiver::BlobMetadata& metadata) {}

// This comment serves as a reminder that the following functions were deprecated, and should be used inline when
// needed. validate_file_mode validate_not_negative validate_file_exists

}  // anonymous namespace

namespace quiver {

struct Blob::Impl {
    std::iostream io;
    std::string file_path;
    BlobMetadata metadata;
};

}  // namespace quiver