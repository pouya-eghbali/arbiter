include_guard(GLOBAL)
include(FetchContent)
# QR decoding only; do not fetch examples, writers, tests or optional codecs.
set(ZXING_READERS ON CACHE BOOL "" FORCE)
set(ZXING_WRITERS OFF CACHE STRING "" FORCE)
set(ZXING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZXING_EXAMPLES_QT OFF CACHE BOOL "" FORCE)
set(ZXING_C_API OFF CACHE BOOL "" FORCE)
set(ZXING_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(ZXING_BLACKBOX_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(zxing
    URL https://codeload.github.com/zxing-cpp/zxing-cpp/tar.gz/refs/tags/v3.1.1
    URL_HASH SHA256=7286b1e6ade66fe82b7c8208b4595deeb55d6486b410834fdc65702f46650542
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
function(arbiter_make_qr_scanner)
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_AUTOMOC OFF)
    set(CMAKE_AUTOUIC OFF)
    FetchContent_MakeAvailable(zxing)
endfunction()
arbiter_make_qr_scanner()
