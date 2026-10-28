find_package(Qt6 ${ARBITER_QT_SERIES} REQUIRED COMPONENTS Quick QuickControls2 Svg)
add_executable(shortcuthelp_test shortcuthelp_test.cpp ../../src/keybindings.cpp ../../src/keybindings.h)
target_link_libraries(shortcuthelp_test PRIVATE arbiter_settings Qt6::Quick Qt6::QuickControls2 Qt6::Svg Qt6::Test)
set(help_qml ../../qml/ShortcutHelp.qml ../../qml/Theme.qml ../../qml/GlassSurface.qml
    ../../qml/ui/UiPopup.qml ../../qml/ui/UiButton.qml ../../qml/ui/UiIconButton.qml
    ../../qml/ui/UiIcon.qml ../../qml/ui/UiToolTip.qml ../../qml/ui/LucideIcons.js)
foreach(help_file IN LISTS help_qml)
    get_filename_component(help_name "${help_file}" NAME)
    set_source_files_properties("${help_file}" PROPERTIES QT_RESOURCE_ALIAS "${help_name}")
endforeach()
set_source_files_properties(../../qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(shortcuthelp_test URI Arbiter VERSION 1.0 NO_IMPORT_SCAN
    RESOURCE_PREFIX /shortcut-help-test QML_FILES ${help_qml})
qt_add_resources(shortcuthelp_test icon_script PREFIX / BASE ../..
    FILES ../../qml/ui/LucideIcons.js)
add_test(NAME shortcuthelp COMMAND shortcuthelp_test)
set_tests_properties(shortcuthelp PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
