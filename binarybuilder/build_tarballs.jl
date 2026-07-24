using BinaryBuilder
using Pkg
using TOML

repository = dirname(@__DIR__)
project_path = joinpath(repository, "bindings", "julia", "Project.toml")
project = TOML.parse(read(project_path, String))
@show name = project["name"]
@show version = VersionNumber(project["version"])

sources = [DirectorySource(repository)]

script = raw"""
apk del cmake
cd ${WORKSPACE}/srcdir
cmake -B build \
    -DCMAKE_INSTALL_PREFIX=${prefix} \
    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TARGET_TOOLCHAIN} \
    -DCMAKE_BUILD_TYPE=Release \
    -DQUIVER_BUILD_TESTS=OFF \
    -DQUIVER_BUILD_C_API=ON \
    -DHAVE_GNU_STRERROR_R_EXITCODE=0
cmake --build build --parallel ${nproc}
cmake --install build
install_license LICENSE
"""

# ONE deterministic tarball: x86_64 Linux, cxx11 std::string ABI (the modern default the code uses,
# _GLIBCXX_USE_CXX11_ABI=1). NOT expand_cxxstring_abis (that would emit cxx11 + cxx03 = two tarballs).

@show platforms = supported_platforms()
@show platforms = expand_cxxstring_abis(platforms)

products = [
    LibraryProduct("libquiver", :libquiver),
    LibraryProduct("libquiver_c", :libquiver_c),
]

dependencies = [
    HostBuildDependency(PackageSpec(; name = "CMake_jll")),
]

# GCC 10 -> GLIBCXX_3.4.28, which stays within the libstdc++ Julia bundles (GCC 12 -> 3.4.30). This
# ships as a plain S3 artifact (no CompilerSupportLibraries_jll to supply a newer libstdc++ at load
# time), so GCC 13 (-> 3.4.32) would fail to load. Do not bump past v"11" on this path.
build_tarballs(ARGS, name, version, sources, script, platforms, products, dependencies;
    julia_compat = "1.7",
    preferred_gcc_version = v"10")
