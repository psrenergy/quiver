#ifndef MARGAUX_ATTRIBUTE_TYPE_H
#define MARGAUX_ATTRIBUTE_TYPE_H

#include "data_type.h"
#include "export.h"

namespace psr {

enum class DataStructure { Scalar, Vector, Set };

struct MARGAUX_API AttributeType {
    DataStructure data_structure;
    DataType data_type;
};

}  // namespace psr

#endif  // MARGAUX_ATTRIBUTE_TYPE_H
