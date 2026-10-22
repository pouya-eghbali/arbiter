cmake_minimum_required(VERSION 3.24)

project(Arbiter VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.12 REQUIRED COMPONENTS Quick QuickControls2 WebEngineQuick Widgets)
qt_standard_project_setup(REQUIRES 6.12)

qt_add_executable(arbiter src/main.cpp src/browserapplication.h src/keybindings.cpp src/keybindings.h)
if(APPLE)
    enable_language(OBJCXX)
    target_sources(arbiter PRIVATE src/macwindow.mm)
    target_link_libraries(arbiter PRIVATE "-framework AppKit")
endif()
qt_add_qml_module(arbiter
    URI Arbiter
    VERSION 1.0
    QML_FILES qml/Main.qml
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
