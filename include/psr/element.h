#ifndef PSR_ELEMENT_H
#define PSR_ELEMENT_H

#include "export.h"
#include "value.h"

#include <map>
#include <string>
#include <vector>

namespace psr {

class PSR_API Element {
public:
    Element() = default;

    // Scalars (fluent)
    Element& set(const std::string& name, int64_t value);
    Element& set(const std::string& name, double value);
    Element& set(const std::string& name, const std::string& value);
    Element& set_null(const std::string& name);

    // Vectors (fluent)
    Element& set_vector(const std::string& name, std::vector<int64_t> values);
    Element& set_vector(const std::string& name, std::vector<double> values);
    Element& set_vector(const std::string& name, std::vector<std::string> values);

    // Accessors
    const std::map<std::string, Value>& scalars() const;
    const std::map<std::string, Value>& vectors() const;

    bool has_scalars() const;
    bool has_vectors() const;

    void clear();

    // Pretty print
    std::string to_string() const;

private:
    std::map<std::string, Value> scalars_;
    std::map<std::string, Value> vectors_;
};

}  // namespace psr

#endif  // PSR_ELEMENT_H
