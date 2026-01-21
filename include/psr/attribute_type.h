#ifndef PSR_ATTRIBUTE_TYPE_H
#define PSR_ATTRIBUTE_TYPE_H

#include "data_type.h"
#include "export.h"

namespace margaux {

enum class DataStructure { Scalar, Vector, Set };

struct PSR_API AttributeType {
    DataStructure data_structure;
    DataType data_type;
};

}  // namespace margaux

#endif  // PSR_ATTRIBUTE_TYPE_H
