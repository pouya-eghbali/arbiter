function(arbiter_add_linux_window_frame target)
    target_sources(${target} PRIVATE
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../src/linuxwindowframe.cpp"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../src/linuxwindowframe.h")
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(Qt6 ${ARBITER_QT_SERIES} REQUIRED COMPONENTS GuiPrivate)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(ARBITER_FRAME_XCB REQUIRED IMPORTED_TARGET xcb xcb-shape)
        target_link_libraries(${target} PRIVATE Qt6::GuiPrivate PkgConfig::ARBITER_FRAME_XCB)
    endif()
endfunction()
