cmake_minimum_required(VERSION 3.24)

project(Arbiter VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/ArbiterWebEngine.cmake)
find_package(Qt6 6.12 REQUIRED COMPONENTS Quick QuickControls2 WebEngineQuick Widgets WebChannel Sql Concurrent Network Positioning)
qt_standard_project_setup(REQUIRES 6.12)
include(cmake/Settings.cmake)
include(cmake/QtKeychain.cmake)
find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)

qt_add_executable(arbiter src/browserimportselection.cpp src/browserimportselection.h src/browsericons.cpp src/browsericons.h src/browserinstance.cpp src/browserinstance.h src/onboarding.cpp src/onboarding.h src/systemintegration.cpp src/systemintegration.h src/browserimporter.cpp src/browserimporter.h src/importdata.cpp src/importdata.h src/settings.h src/splitswapgesture.cpp src/splitswapgesture.h src/splitlayout.cpp src/splitlayout.h src/workspaces.cpp src/workspaces.h src/webprofiles.h src/history.cpp src/history.h src/main.cpp src/appearance.cpp src/appearance.h src/sitenavigation.cpp src/sitenavigation.h src/browserapplication.h src/keybindings.cpp src/keybindings.h src/tabs.cpp src/tabs.h
    src/passwordmanager.cpp src/passwordmanager.h src/secretstore.cpp src/secretstore.h
    src/webauthncontroller.cpp src/webauthncontroller.h
    src/passkeycontroller.cpp src/passkeycontroller.h src/passkeyvault.cpp src/passkeyvault.h src/lockedkey.cpp src/lockedkey.h
    src/passkeycrypto.cpp src/passkeycrypto.h third_party/qrcodegen/qrcodegen.cpp)
include(cmake/BrowserImport.cmake)
arbiter_add_browser_import(arbiter)
include(cmake/Biometrics.cmake)
arbiter_add_biometrics(arbiter)
target_sources(arbiter PRIVATE src/privatebrowsing.cpp src/privatebrowsing.h src/tabsleepsettings.cpp src/tabsleepsettings.h)
target_sources(arbiter PRIVATE src/sitepermissions.cpp src/sitepermissions.h)
target_include_directories(arbiter PRIVATE src third_party/qrcodegen)
arbiter_configure_webengine_runtime(arbiter)
qt_add_resources(arbiter password_scripts PREFIX /passwords BASE scripts FILES scripts/passwords.js)
qt_add_resources(arbiter brand PREFIX /brand BASE assets/icon/Arbiter.icon/Assets FILES assets/icon/Arbiter.icon/Assets/valkyrie-source.png)
qt_add_resources(arbiter navigation_scripts PREFIX /navigation BASE scripts FILES scripts/navigation.js)
if(APPLE)
    enable_language(OBJCXX)
    target_sources(arbiter PRIVATE src/macwindow.mm src/systemintegration_mac.mm src/browsericons_mac.mm)
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
    set_source_files_properties(licenses/QtKeychain.txt licenses/QRCodeGenerator.txt licenses/TomlPlusPlus.txt PROPERTIES MACOSX_PACKAGE_LOCATION Resources/licenses)
    target_sources(arbiter PRIVATE
        "${ARBITER_ICON_OUTPUT}/Assets.car" "${ARBITER_ICON_OUTPUT}/Arbiter.icns"
        licenses/QtKeychain.txt licenses/QRCodeGenerator.txt licenses/TomlPlusPlus.txt
    )
endif()
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(arbiter
    # Dynamic Qt resolves plugins at runtime. Avoid recursively scanning the
    # isolated Chromium checkout beneath this source directory.
    NO_IMPORT_SCAN
    URI Arbiter
    VERSION 1.0
    QML_FILES qml/PermissionPanel.qml qml/DesktopSharePanel.qml qml/SitePermissionsSettings.qml qml/AddressPrompt.qml qml/BrowserBackground.qml qml/ui/UiImagePicker.qml qml/PrivateShield.qml qml/SplitWithPicker.qml qml/TabSleepController.qml qml/ui/UiIcon.qml qml/ui/UiChoiceCard.qml qml/ImportProgress.qml qml/BrowserAccess.qml qml/OnboardingImport.qml qml/Onboarding.qml qml/BrowserPreview.qml qml/Tutorial.qml qml/ImportPage.qml qml/StartPage.qml qml/PaneShape.qml qml/SplitOverlay.qml qml/BiometricSetup.qml qml/WorkspacePanel.qml qml/HistoryPanel.qml qml/Main.qml qml/Theme.qml qml/ui/UiButton.qml qml/ui/UiTextField.qml qml/ui/UiCheckBox.qml qml/ui/UiComboBox.qml qml/ui/UiToolTip.qml qml/ui/UiMenuItem.qml qml/ui/UiColorPicker.qml qml/GlassSurface.qml qml/NavigationPill.qml qml/WebContextMenu.qml qml/PasswordPanel.qml qml/WebAuthnPanel.qml qml/PasskeySetup.qml
)
target_link_libraries(arbiter PRIVATE
    arbiter_settings
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::WebEngineQuick
    Qt6::Widgets
    Qt6::WebChannel
    Qt6::Sql
    Qt6::Concurrent
    Qt6::Network
    Qt6::Positioning
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

if(APPLE)
    # The linker's executable-only signature does not seal the app's plist and
    # resources. Keychain must be able to validate the complete calling bundle.
    # A certificate identity also preserves app identity across rebuilds; ad-hoc
    # signing is suitable for local builds, but changes identity when code changes.
    set(ARBITER_MACOS_SIGNING_IDENTITY "-" CACHE STRING "macOS signing identity (- for local ad-hoc signing)")
    find_program(ARBITER_CODESIGN_EXECUTABLE codesign REQUIRED)
    add_custom_command(TARGET arbiter POST_BUILD
        COMMAND "${ARBITER_CODESIGN_EXECUTABLE}" --force --sign "${ARBITER_MACOS_SIGNING_IDENTITY}"
            "$<TARGET_BUNDLE_DIR:arbiter>"
        COMMAND "${ARBITER_CODESIGN_EXECUTABLE}" --verify --strict "$<TARGET_BUNDLE_DIR:arbiter>"
        VERBATIM
        COMMENT "Signing and verifying the macOS app bundle"
    )
endif()

if(WIN32)
    target_link_libraries(arbiter PRIVATE shlwapi)
endif()

include(GNUInstallDirs)
install(TARGETS arbiter
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(NOT APPLE)
    install(FILES licenses/QtKeychain.txt licenses/QRCodeGenerator.txt licenses/TomlPlusPlus.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/arbiter/licenses)
endif()
if(UNIX AND NOT APPLE)
    install(FILES cmake/com.arbiter.browser.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
endif()
