# Prefer a distribution-provided Qt 6 build. Otherwise build the pinned, BSD-licensed
# dependency statically, so packaging does not need another shared library.
include(${CMAKE_CURRENT_LIST_DIR}/Dependencies.cmake)
option(ARBITER_BUNDLED_KEYCHAIN "Build the pinned static QtKeychain (release builds)" OFF)
if(NOT ARBITER_BUNDLED_KEYCHAIN)
    find_package(Qt6Keychain ${ARBITER_KEYCHAIN_VERSION} QUIET CONFIG)
endif()
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
        URL https://codeload.github.com/frankosterfeld/qtkeychain/tar.gz/refs/tags/${ARBITER_KEYCHAIN_VERSION}
        URL_HASH SHA256=${ARBITER_KEYCHAIN_SHA256}
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

function(arbiter_add_mac_secretstore target)
    if(APPLE)
        set(source "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../src/secretstore_mac.mm")
        set_source_files_properties(${source} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
        target_sources(${target} PRIVATE ${source})
        target_link_libraries(${target} PRIVATE "-framework Security" "-framework Foundation")
    endif()
endfunction()
