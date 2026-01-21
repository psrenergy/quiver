#ifndef DECK_DATABASE_RESULT_H
#define DECK_DATABASE_RESULT_H

#include "export.h"
#include "row.h"

#include <vector>

namespace margaux {

class DECK_DATABASE_API Result {
public:
    Result();  // Default constructor for empty result
    Result(std::vector<std::string> columns, std::vector<Row> rows);

    const std::vector<std::string>& columns() const;
    size_t column_count() const;
    size_t row_count() const;
    bool empty() const;

    const Row& at(size_t index) const;
    const Row& operator[](size_t index) const;

    // Iterator support
    auto begin() const { return rows_.begin(); }
    auto end() const { return rows_.end(); }

private:
    std::vector<std::string> columns_;
    std::vector<Row> rows_;
};

}  // namespace margaux

#endif  // DECK_DATABASE_RESULT_H
