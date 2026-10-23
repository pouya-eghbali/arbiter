include_guard(GLOBAL)
add_library(arbiter_settings STATIC
    "${CMAKE_CURRENT_LIST_DIR}/../src/settings.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/settings.h")
target_link_libraries(arbiter_settings PUBLIC Qt6::Core)
target_include_directories(arbiter_settings SYSTEM PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../third_party/tomlplusplus")
