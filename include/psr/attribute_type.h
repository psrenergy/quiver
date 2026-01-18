#ifndef PSR_ATTRIBUTE_TYPE_H
#define PSR_ATTRIBUTE_TYPE_H

#include "data_type.h"
#include "export.h"

namespace psr {

enum class AttributeStructure { Scalar, Vector, Set };

struct PSR_API AttributeType {
    AttributeStructure structure;
    DataType data_type;
};

}  // namespace psr

#endif  // PSR_ATTRIBUTE_TYPE_H
