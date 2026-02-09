#ifndef QUIVER_CSV_H
#define QUIVER_CSV_H

#include "data_type.h"
#include "enum_map.h"
#include "result.h"
#include "value.h"

#include <map>
#include <string>
#include <vector>

namespace quiver {

using DateFormatMap = std::map<std::string, std::string>;
using FkLabelMap = std::map<std::string, std::map<int64_t, std::string>>;
using ColumnTypeMap = std::map<std::string, DataType>;

struct CsvData {
    std::vector<std::string> columns;
    std::vector<std::vector<Value>> rows;
};

void write_csv(const std::string& path,
               const std::vector<std::string>& columns,
               const Result& data,
               const FkLabelMap& fk_labels = {},
               const DateFormatMap& date_format_map = {},
               const EnumMap& enum_map = {});

CsvData read_csv(const std::string& path,
                 const ColumnTypeMap& column_types = {},
                 const FkLabelMap& fk_labels = {},
                 const DateFormatMap& date_format_map = {},
                 const EnumMap& enum_map = {});

}  // namespace quiver

#endif  // QUIVER_CSV_H
