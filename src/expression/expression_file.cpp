#include "quiver/binary/binary_file.h"
#include "quiver/expression/expression_node.h"

#include <cstdint>
#include <string>
#include <vector>

namespace quiver {

ExpressionFile::ExpressionFile(const std::string& path) : meta_(BinaryMetadata::from_toml_file(path)), file_(path) {
    dim_map_.reserve(meta_.dimensions.size());
}

const BinaryMetadata& ExpressionFile::metadata() const {
    return meta_;
}

void ExpressionFile::compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const {
    for (size_t i = 0; i < meta_.dimensions.size(); ++i) {
        dim_map_[meta_.dimensions[i].name] = dims[i];
    }
    out = file_.read(dim_map_, /*allow_nulls=*/true);
}

void ExpressionFile::collect_input_files(std::vector<BinaryFile*>& out) const {
    out.push_back(&file_);
}

}  // namespace quiver
