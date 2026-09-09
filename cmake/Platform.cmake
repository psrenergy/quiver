# Platform-specific configuration

# macOS deployment floor. libc++ marks the floating-point std::to_chars used by
# database_csv_export.cpp and lua_runner.cpp unavailable before macOS 13.3, so the floor belongs
# to the core, not to one binding. Without it clang stamps the *builder's* OS version into every
# dylib, so the published Julia/JS/S3 natives silently required whatever the CI runner image
# happened to be that week -- and any build that lands below 13.3 (a MACOSX_DEPLOYMENT_TARGET
# exported by Homebrew/conda, or cibuildwheel) fails on the to_chars availability gate instead.
# It is a floor, not an override: a higher explicit value is respected. The Dart hook still needs
# its own DEPLOYMENT_TARGET, because the iOS toolchain file force-derives
# CMAKE_OSX_DEPLOYMENT_TARGET from it before this file is read.
if(APPLE)
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS 13.3)
        set(CMAKE_OSX_DEPLOYMENT_TARGET 13.3 CACHE STRING "Minimum macOS version" FORCE)
    endif()
endif()

# RPATH settings for Linux/macOS
# This ensures executables can find shared libraries both during build and after install
if(NOT WIN32)
    # Include RPATH in build tree (required for running from build directory)
    set(CMAKE_SKIP_BUILD_RPATH FALSE)

    # Don't use install RPATH during build - use build tree paths instead
    set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

    # Set install RPATH to find libs relative to executable location
    # $ORIGIN = directory containing the executable
    if(APPLE)
        set(CMAKE_INSTALL_RPATH "@executable_path/../lib")
    else()
        set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
    endif()

    # Add library directories from the linker search path to RPATH
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

# Hide symbols by default (explicit export required)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
