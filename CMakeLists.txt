cmake_minimum_required(VERSION 3.24)

project(Arbiter VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/ArbiterWebEngine.cmake)
find_package(Qt6 6.12 REQUIRED COMPONENTS Quick QuickControls2 WebEngineQuick Widgets WebChannel Sql)
qt_standard_project_setup(REQUIRES 6.12)
include(cmake/QtKeychain.cmake)
find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)

qt_add_executable(arbiter src/workspaces.cpp src/workspaces.h src/webprofiles.h src/history.cpp src/history.h src/main.cpp src/appearance.cpp src/appearance.h src/sitenavigation.cpp src/sitenavigation.h src/browserapplication.h src/keybindings.cpp src/keybindings.h src/tabs.cpp src/tabs.h
    src/passwordmanager.cpp src/passwordmanager.h src/secretstore.cpp src/secretstore.h
    src/webauthncontroller.cpp src/webauthncontroller.h
    src/passkeycontroller.cpp src/passkeycontroller.h src/passkeyvault.cpp src/passkeyvault.h
    src/passkeycrypto.cpp src/passkeycrypto.h third_party/qrcodegen/qrcodegen.cpp)
target_include_directories(arbiter PRIVATE third_party/qrcodegen)
arbiter_configure_webengine_runtime(arbiter)
qt_add_resources(arbiter password_scripts PREFIX /passwords BASE scripts FILES scripts/passwords.js)
qt_add_resources(arbiter navigation_scripts PREFIX /navigation BASE scripts FILES scripts/navigation.js)
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
    set_source_files_properties(licenses/QtKeychain.txt licenses/QRCodeGenerator.txt PROPERTIES MACOSX_PACKAGE_LOCATION Resources/licenses)
    target_sources(arbiter PRIVATE
        "${ARBITER_ICON_OUTPUT}/Assets.car" "${ARBITER_ICON_OUTPUT}/Arbiter.icns"
        licenses/QtKeychain.txt licenses/QRCodeGenerator.txt
    )
endif()
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(arbiter
    # Dynamic Qt resolves plugins at runtime. Avoid recursively scanning the
    # isolated Chromium checkout beneath this source directory.
    NO_IMPORT_SCAN
    URI Arbiter
    VERSION 1.0
    QML_FILES qml/WorkspacePanel.qml qml/HistoryPanel.qml qml/Main.qml qml/Theme.qml qml/ui/UiButton.qml qml/ui/UiTextField.qml qml/ui/UiCheckBox.qml qml/ui/UiComboBox.qml qml/ui/UiToolTip.qml qml/ui/UiMenuItem.qml qml/ui/UiColorPicker.qml qml/GlassSurface.qml qml/NavigationPill.qml qml/WebContextMenu.qml qml/PasswordPanel.qml qml/WebAuthnPanel.qml qml/PasskeySetup.qml
)
target_link_libraries(arbiter PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::WebEngineQuick
    Qt6::Widgets
    Qt6::WebChannel
    Qt6::Sql
    qt6keychain
    OpenSSL::Crypto
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
if(NOT APPLE)
    install(FILES licenses/QtKeychain.txt licenses/QRCodeGenerator.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/arbiter/licenses)
endif()
