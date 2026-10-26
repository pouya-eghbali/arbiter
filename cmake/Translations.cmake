# Bundle app and Qt UI catalogs. No runtime network or external catalog path.
function(arbiter_add_translations target)
    find_package(Qt6 ${ARBITER_QT_SERIES} REQUIRED COMPONENTS LinguistTools)
    set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
    file(READ "${root}/translations/languages.json" languages)
    string(JSON count LENGTH "${languages}")
    math(EXPR last "${count} - 1")
    set(catalogs "")
    foreach(index RANGE ${last})
        string(JSON code GET "${languages}" ${index} code)
        if(NOT code STREQUAL "en")
            list(APPEND catalogs "${root}/translations/arbiter_${code}.ts")
        endif()
    endforeach()
    # Include every platform's sources even when updating on a different OS.
    file(GLOB sources CONFIGURE_DEPENDS "${root}/src/*.cpp" "${root}/src/*.h" "${root}/src/*.mm"
        "${root}/qml/*.qml" "${root}/qml/ui/*.qml")
    # Use explicit sources rather than all target resources: licenses, browser
    # scripts, and generated code are not input for Qt's translation parser.
    qt_add_lupdate(
        TS_FILES ${catalogs}
        SOURCES ${sources}
        LUPDATE_TARGET ${target}_lupdate
        OPTIONS -no-obsolete -locations relative -extensions cpp,h,mm,qml,js)
    qt_add_lrelease(
        TS_FILES ${catalogs}
        LRELEASE_TARGET ${target}_lrelease
        QM_FILES_OUTPUT_VARIABLE compiled_catalogs
        OPTIONS -nounfinished
        MERGE_QT_TRANSLATIONS
        QT_TRANSLATION_CATALOGS qtbase qtdeclarative qtwebengine)
    qt_add_resources(${target} "${target}_translations"
        PREFIX /i18n BASE "${CMAKE_CURRENT_BINARY_DIR}" FILES ${compiled_catalogs})
endfunction()
