cmake -B build -DCMAKE_BUILD_TYPE=Debug -DDECK_DATABASE_BUILD_TESTS=ON -DDECK_DATABASE_BUILD_C_API=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure