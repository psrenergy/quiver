#ifndef QUIVER_ATTRIBUTE_TYPE_H
#define QUIVER_ATTRIBUTE_TYPE_H

#include "data_type.h"
#include "export.h"

namespace quiver {

enum class DataStructure { Scalar, Vector, Set };

struct QUIVER_API AttributeType {
    DataStructure data_structure;
    DataType data_type;
};

}  // namespace quiver

#endif  // QUIVER_ATTRIBUTE_TYPE_H
