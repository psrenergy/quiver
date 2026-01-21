#ifndef DECK_DATABASE_ELEMENT_H
#define DECK_DATABASE_ELEMENT_H

#include "export.h"
#include "value.h"

#include <map>
#include <string>
#include <vector>

namespace margaux {

class DECK_DATABASE_API Element {
public:
    Element() = default;

    // Scalars
    Element& set(const std::string& name, int64_t value);
    Element& set(const std::string& name, double value);
    Element& set(const std::string& name, const std::string& value);
    Element& set_null(const std::string& name);

    // Arrays - stored generically, Database::create_element routes to vector/set tables
    Element& set(const std::string& name, const std::vector<int64_t>& values);
    Element& set(const std::string& name, const std::vector<double>& values);
    Element& set(const std::string& name, const std::vector<std::string>& values);

    // Accessors
    const std::map<std::string, Value>& scalars() const;
    const std::map<std::string, std::vector<Value>>& arrays() const;

    bool has_scalars() const;
    bool has_arrays() const;

    void clear();

    // Pretty print
    std::string to_string() const;

private:
    std::map<std::string, Value> scalars_;
    std::map<std::string, std::vector<Value>> arrays_;
};

}  // namespace margaux

#endif  // DECK_DATABASE_ELEMENT_H
