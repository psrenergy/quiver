#ifndef QUIVER_CSV_CONVERTER_H
#define QUIVER_CSV_CONVERTER_H

#include "../export.h"

#include <string>

namespace quiver {

// `file_path` names the .qvr/.toml pair without extension; the CSV is `file_path` + ".csv".
QUIVER_API void bin_to_csv(const std::string& file_path, bool aggregate_time_dimensions = true);
QUIVER_API void csv_to_bin(const std::string& file_path);

}  // namespace quiver

#endif  // QUIVER_CSV_CONVERTER_H
