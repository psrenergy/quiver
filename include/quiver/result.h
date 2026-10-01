#ifndef QUIVER_RESULT_H
#define QUIVER_RESULT_H

#include "export.h"
#include "row.h"

#include <vector>

namespace quiver {

class QUIVER_API Result {
public:
    Result(std::vector<std::string> columns, std::vector<Row> rows);

    const std::vector<std::string>& columns() const;
    size_t row_count() const;
    bool empty() const;

    const Row& operator[](size_t index) const;

    // Iterator support
    auto begin() const { return rows_.begin(); }
    auto end() const { return rows_.end(); }

private:
    std::vector<std::string> columns_;
    std::vector<Row> rows_;
};

}  // namespace quiver

#endif  // QUIVER_RESULT_H
