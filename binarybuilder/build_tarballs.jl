using BinaryBuilder
using Pkg
using TOML

repository = dirname(@__DIR__)
project_path = joinpath(repository, "bindings", "julia", "Project.toml")
project = TOML.parse(read(project_path, String))
@show name = project["name"]
@show version = VersionNumber(project["version"])

staging = mktempdir()
run(pipeline(`git -C $repository archive --format=tar HEAD`, `tar -x -C $staging`))
sources = [DirectorySource(staging)]

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

# @show platforms = supported_platforms()
# @show platforms = expand_cxxstring_abis(platforms)

platforms = [
    Platform("x86_64", "linux"; libc = "glibc", cxxstring_abi = "cxx11")
]

products = [
    LibraryProduct("libquiver", :libquiver),
    LibraryProduct("libquiver_c", :libquiver_c),
]

dependencies = [
    HostBuildDependency(PackageSpec(; name = "CMake_jll")),
]

# Toolchain: GCC 11. BinaryBuilder links Linux glibc targets against an old glibc baseline (well
# below 2.17), independent of the GCC version -- that is what makes the resulting libquiver.so
# compatible with glibc-2.17 systems (RHEL/CentOS 7 and newer), the goal of this recipe. GCC 11 is
# the *minimum* that compiles the code: src/utils/datetime.h and src/binary/binary_utils.h use
# C++20 <chrono> calendar types (year_month_day, sys_days, hh_mm_ss) that libstdc++ implements only
# from GCC 11. (Bonus: GCC 11 keeps the C++ runtime requirement at GLIBCXX_3.4.29, which the
# libstdc++ that Julia bundles (>= 1.7) still provides, so the artifact also loads under Julia.)
build_tarballs(ARGS, name, version, sources, script, platforms, products, dependencies;
    julia_compat = "1.11",
    preferred_gcc_version = v"11")
