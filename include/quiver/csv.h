#ifndef QUIVER_CSV_H
#define QUIVER_CSV_H

#include "result.h"
#include "value.h"

#include <map>
#include <string>
#include <vector>

namespace quiver {

using DateFormatMap = std::map<std::string, std::string>;
using EnumMap = std::map<std::string, std::map<int64_t, std::string>>;
using FkLabelMap = std::map<std::string, std::map<int64_t, std::string>>;

void write_csv(const std::string& path,
               const std::vector<std::string>& columns,
               const Result& data,
               const FkLabelMap& fk_labels = {},
               const DateFormatMap& date_format_map = {},
               const EnumMap& enum_map = {});

}  // namespace quiver

#endif  // QUIVER_CSV_H
