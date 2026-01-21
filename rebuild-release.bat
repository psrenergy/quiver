cmake -B build -DCMAKE_BUILD_TYPE=Release -DMARGAUX_BUILD_C_API=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure