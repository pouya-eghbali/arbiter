add_executable(extensionpanel_test extensionpanel_test.cpp)
target_link_libraries(extensionpanel_test PRIVATE Qt6::Quick Qt6::WebEngineQuick Qt6::Test)
set(panel_qml
    ../../qml/ExtensionPanel.qml ../../qml/Theme.qml ../../qml/GlassSurface.qml
    ../../qml/ui/UiPopup.qml ../../qml/ui/UiIconButton.qml ../../qml/ui/UiButton.qml
    ../../qml/ui/UiIcon.qml ../../qml/ui/UiToolTip.qml ../../qml/ui/LucideIcons.js)
foreach(panel_file IN LISTS panel_qml)
    get_filename_component(panel_name "${panel_file}" NAME)
    set_source_files_properties("${panel_file}" PROPERTIES QT_RESOURCE_ALIAS "${panel_name}")
endforeach()
set_source_files_properties(../../qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(extensionpanel_test URI Arbiter VERSION 1.0 NO_IMPORT_SCAN
    RESOURCE_PREFIX /extension-panel-test QML_FILES ${panel_qml})
arbiter_configure_webengine_runtime(extensionpanel_test)
add_test(NAME extensionpanel COMMAND extensionpanel_test)
set_tests_properties(extensionpanel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
# UiIcon uses the same absolute script resource as the application.
qt_add_resources(extensionpanel_test icon_script PREFIX / BASE ../..
    FILES ../../qml/ui/LucideIcons.js)
