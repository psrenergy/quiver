# Platform-specific configuration

if(WIN32)
    set(MARGAUX_PLATFORM "windows")
    set(MARGAUX_LIB_PREFIX "")
    set(MARGAUX_LIB_SUFFIX ".dll")
    set(MARGAUX_STATIC_LIB_SUFFIX ".lib")
elseif(APPLE)
    set(MARGAUX_PLATFORM "macos")
    set(MARGAUX_LIB_PREFIX "lib")
    set(MARGAUX_LIB_SUFFIX ".dylib")
    set(MARGAUX_STATIC_LIB_SUFFIX ".a")
else()
    set(MARGAUX_PLATFORM "linux")
    set(MARGAUX_LIB_PREFIX "lib")
    set(MARGAUX_LIB_SUFFIX ".so")
    set(MARGAUX_STATIC_LIB_SUFFIX ".a")
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
