#ifndef PSR_ELEMENT_H
#define PSR_ELEMENT_H

#include "export.h"
#include "value.h"

#include <map>
#include <string>
#include <vector>

namespace psr {

// A row in a vector group: map of column names to values
using VectorRow = std::map<std::string, Value>;

// A vector group: group name -> list of rows (ordered by index)
using VectorGroup = std::vector<VectorRow>;

// A set group: group name -> list of rows (unordered, unique)
using SetGroup = std::vector<VectorRow>;

class PSR_API Element {
public:
    Element() = default;

    // Scalars (fluent)
    Element& set(const std::string& name, int64_t value);
    Element& set(const std::string& name, double value);
    Element& set(const std::string& name, const std::string& value);
    Element& set_null(const std::string& name);

    // Vector groups (fluent) - for tables like Collection_vector_group
    Element& add_vector_group(const std::string& group_name, VectorGroup rows);

    // Set groups (fluent) - for tables like Collection_set_group
    Element& add_set_group(const std::string& group_name, SetGroup rows);

    // Accessors
    const std::map<std::string, Value>& scalars() const;
    const std::map<std::string, VectorGroup>& vector_groups() const;
    const std::map<std::string, SetGroup>& set_groups() const;

    bool has_scalars() const;
    bool has_vector_groups() const;
    bool has_set_groups() const;

    void clear();

    // Pretty print
    std::string to_string() const;

private:
    std::map<std::string, Value> scalars_;
    std::map<std::string, VectorGroup> vector_groups_;
    std::map<std::string, SetGroup> set_groups_;
};

}  // namespace psr

#endif  // PSR_ELEMENT_H
