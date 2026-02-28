#include <iostream>
#include <quiver/c/options.h>
#include <quiver/quiver.h>
#include <string>
#include <vector>

// This is a sandbox for quick prototyping and testing. It is not intended to be a comprehensive test suite.
// cmake --build build --config Debug --target quiver_sandbox
// ./build/Debug/quiver_sandbox.exe

int main() {
    auto db = quiver::Database::from_schema(
        ":memory:", "tests/schemas/valid/basic.sql", {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element element;
    element.set("label", std::string("Test Config")).set("integer_attribute", int64_t{42}).set("float_attribute", 3.14);

    int64_t id = db.create_element("Configuration", element);
    auto labels = db.read_scalar_strings("Configuration", "label");

    std::cout << "Created element with ID: " << id << std::endl;
    std::cout << "Label: " << labels[0] << std::endl;

    auto csv_path = "test_export.csv";
    db.export_csv("Configuration", "", csv_path);
    std::cout << "Exported CSV to: " << csv_path << std::endl;

    return 0;
}