#ifndef DECK_DATABASE_ATTRIBUTE_TYPE_H
#define DECK_DATABASE_ATTRIBUTE_TYPE_H

#include "data_type.h"
#include "export.h"

namespace margaux {

enum class DataStructure { Scalar, Vector, Set };

struct DECK_DATABASE_API AttributeType {
    DataStructure data_structure;
    DataType data_type;
};

}  // namespace margaux

#endif  // DECK_DATABASE_ATTRIBUTE_TYPE_H
