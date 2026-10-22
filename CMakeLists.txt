cmake_minimum_required(VERSION 3.24)

project(Arbiter VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.12 REQUIRED COMPONENTS Quick QuickControls2 WebEngineQuick Widgets)
qt_standard_project_setup(REQUIRES 6.12)

qt_add_executable(arbiter src/main.cpp src/browserapplication.h src/keybindings.cpp src/keybindings.h src/tabs.cpp src/tabs.h)
if(APPLE)
    enable_language(OBJCXX)
    target_sources(arbiter PRIVATE src/macwindow.mm)
    target_link_libraries(arbiter PRIVATE "-framework AppKit")

    find_program(XCRUN_EXECUTABLE xcrun REQUIRED)
    set(ARBITER_ICON_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/Arbiter.icon")
    set(ARBITER_ICON_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/app-icon")
    file(GLOB_RECURSE ARBITER_ICON_INPUTS CONFIGURE_DEPENDS "${ARBITER_ICON_SOURCE}/*")
    set(ARBITER_ICON_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    if(NOT ARBITER_ICON_DEPLOYMENT_TARGET)
        set(ARBITER_ICON_DEPLOYMENT_TARGET "13.0")
    endif()
    add_custom_command(
        OUTPUT "${ARBITER_ICON_OUTPUT}/Assets.car" "${ARBITER_ICON_OUTPUT}/Arbiter.icns"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARBITER_ICON_OUTPUT}"
        COMMAND "${XCRUN_EXECUTABLE}" actool "${ARBITER_ICON_SOURCE}"
            --compile "${ARBITER_ICON_OUTPUT}"
            --platform macosx
            --minimum-deployment-target "${ARBITER_ICON_DEPLOYMENT_TARGET}"
            --app-icon Arbiter
            --output-partial-info-plist "${ARBITER_ICON_OUTPUT}/partial.plist"
        DEPENDS ${ARBITER_ICON_INPUTS}
        VERBATIM
        COMMENT "Compiling Arbiter's layered macOS app icon"
    )
    set_source_files_properties(
        "${ARBITER_ICON_OUTPUT}/Assets.car" "${ARBITER_ICON_OUTPUT}/Arbiter.icns"
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources
    )
    target_sources(arbiter PRIVATE
        "${ARBITER_ICON_OUTPUT}/Assets.car" "${ARBITER_ICON_OUTPUT}/Arbiter.icns"
    )
endif()
qt_add_qml_module(arbiter
    URI Arbiter
    VERSION 1.0
    QML_FILES qml/Main.qml qml/WebContextMenu.qml
)
target_link_libraries(arbiter PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::WebEngineQuick
    Qt6::Widgets
)
set_target_properties(arbiter PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME Arbiter
    MACOSX_BUNDLE_GUI_IDENTIFIER com.arbiter.browser
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS arbiter
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
