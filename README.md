# PSR Database

[![CI](https://github.com/psrenergy/database/actions/workflows/ci.yml/badge.svg)](https://github.com/psrenergy/database/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/psrenergy/database/graph/badge.svg?token=TwhY6YNDNN)](https://codecov.io/gh/psrenergy/database)

<!-- # Rebuild only changed files (fast)
cmake --build build --config Release

# Run all tests
ctest --test-dir build -C Release

# Or combine both
cmake --build build --config Release && ctest --test-dir build -C Release

Faster test options:
# Run tests in parallel (uses all cores)
ctest --test-dir build -C Release -j

# Run specific test by name pattern
ctest --test-dir build -C Release -R "CApiTest"

# Run single test
ctest --test-dir build -C Release -R "CApiTest.OpenAndClose"

# Show output only on failure (less noise)
ctest --test-dir build -C Release --output-on-failure
Only reconfigure if CMakeLists.txt changed:
cmake -B build -DCMAKE_BUILD_TYPE=Release -DPSR_BUILD_C_API=ON -->