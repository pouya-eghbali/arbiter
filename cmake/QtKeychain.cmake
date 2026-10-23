# Prefer a distribution-provided Qt 6 build. Otherwise build the pinned, BSD-licensed
# dependency statically, so packaging does not need another shared library.
find_package(Qt6Keychain 0.14.2 QUIET CONFIG)
if(APPLE)
    enable_language(OBJCXX)
endif()
if(NOT TARGET qt6keychain)
    include(FetchContent)
    set(BUILD_WITH_QT6 ON CACHE BOOL "" FORCE)
    set(BUILD_TRANSLATIONS OFF CACHE BOOL "" FORCE)
    set(BUILD_TEST_APPLICATION OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF)
    FetchContent_Declare(qtkeychain
        URL https://codeload.github.com/frankosterfeld/qtkeychain/tar.gz/refs/tags/0.14.2
        URL_HASH SHA256=cf2e972b783ba66334a79a30f6b3a1ea794a1dc574d6c3bebae5ffd2f0399571
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(qtkeychain)
    target_include_directories(qt6keychain PUBLIC
        "$<BUILD_INTERFACE:${qtkeychain_SOURCE_DIR}>"
        "$<BUILD_INTERFACE:${qtkeychain_BINARY_DIR}>")
else()
    find_path(ARBITER_KEYCHAIN_INCLUDE_DIR keychain.h
        HINTS ${QTKEYCHAIN_INCLUDE_DIRS} PATH_SUFFIXES qt6keychain REQUIRED)
    target_include_directories(qt6keychain INTERFACE "${ARBITER_KEYCHAIN_INCLUDE_DIR}")
endif()
