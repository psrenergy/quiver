#include <iostream>
#include <quiver/quiver.h>
#include <string>
#include <vector>

// This is a sandbox for quick prototyping and testing. It is not intended to be a comprehensive test suite.
// cmake --build build --config Debug --target quiver_sandbox
// ./build/bin/Debug/quiver_sandbox.exe

int main() {
    try {
        // Build metadata: 2 dimensions (block x stage), 3 labels
        auto metadata = quiver::BinaryMetadata::from_element(quiver::Element()
                                                                 .set("version", "1")
                                                                 .set("initial_datetime", "2025-01-01T00:00:00")
                                                                 .set("unit", "MW")
                                                                 .set("dimensions", {"stage", "block"})
                                                                 .set("dimension_sizes", {4, 31})
                                                                 .set("time_dimensions", {"stage", "block"})
                                                                 .set("frequencies", {"monthly", "daily"})
                                                                 .set("labels", {"plant_1", "plant_2", "plant_3"}));

        std::vector<int64_t> blocks_per_stage = {31, 28, 31, 28};

        // Write binary file
        std::string file_path = "sandbox_output";
        auto binary = quiver::BinaryFile::open_file(file_path, 'w', metadata);

        for (int64_t stage = 4; stage >= 1; --stage) {
            for (int64_t block = 1; block <= blocks_per_stage[stage - 1]; ++block) {
                double aux = block + stage / 10.0;
                std::vector<double> data = {aux, aux * 2, aux * 3};
                binary.write(data, {{"block", block}, {"stage", stage}});
            }
        }

        std::cout << "Wrote " << file_path << ".qvr" << std::endl;

        // Read it back
        auto reader = quiver::BinaryFile::open_file(file_path, 'r');

        auto values = reader.read({{"block", 28}, {"stage", 4}});
        std::cout << "Read block=28, stage=4:";
        for (double v : values) {
            std::cout << " " << v;
        }
        std::cout << std::endl;

        auto values_2 = reader.read({{"block", 30}, {"stage", 4}}, true);
        std::cout << "Read block=30, stage=4:";
        for (double v : values_2) {
            std::cout << " " << v;
        }
        std::cout << std::endl;

        quiver::CSVConverter::bin_to_csv(file_path);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
