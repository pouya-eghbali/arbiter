add_executable(glasssurface_test glasssurface_test.cpp)
target_link_libraries(glasssurface_test PRIVATE Qt6::Quick Qt6::Test)
set_source_files_properties(../../qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE QT_RESOURCE_ALIAS qml/Theme.qml)
set_source_files_properties(../../qml/GlassSurface.qml PROPERTIES QT_RESOURCE_ALIAS qml/GlassSurface.qml)
qt_add_qml_module(glasssurface_test URI Arbiter VERSION 1.0 NO_IMPORT_SCAN
    RESOURCE_PREFIX /glass-test
    QML_FILES ../../qml/GlassSurface.qml ../../qml/Theme.qml)
add_test(NAME glasssurface COMMAND glasssurface_test)
set_tests_properties(glasssurface PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
