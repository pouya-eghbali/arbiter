option(ARBITER_PATCHED_WEBENGINE "Use Arbiter ABI 1 patched Qt WebEngine" OFF)
option(ARBITER_WEBENGINE_SDK_RUNTIME "Run directly from the selected SDK (disable for a deployed bundle)" ON)
set(ARBITER_WEBENGINE_PREFIX "" CACHE PATH "Installed Arbiter WebEngine SDK")
if(ARBITER_PATCHED_WEBENGINE)
    if(NOT EXISTS "${ARBITER_WEBENGINE_PREFIX}/arbiter-engine-manifest.json")
        message(FATAL_ERROR "Set ARBITER_WEBENGINE_PREFIX to the installed SDK produced by engine/build.py.")
    endif()
    file(READ "${ARBITER_WEBENGINE_PREFIX}/arbiter-engine-manifest.json" arbiter_engine_manifest)
    string(JSON arbiter_engine_abi GET "${arbiter_engine_manifest}" abi)
    if(NOT arbiter_engine_abi EQUAL 1)
        message(FATAL_ERROR "The selected WebEngine SDK has an unsupported Arbiter ABI.")
    endif()
    string(JSON arbiter_view_interceptor ERROR_VARIABLE arbiter_hook_error GET "${arbiter_engine_manifest}" view_request_interceptor)
    if(arbiter_hook_error OR NOT arbiter_view_interceptor)
        message(FATAL_ERROR "Rebuild the current engine patches: this SDK lacks the per-view ad-block interception hook.")
    endif()
    string(JSON arbiter_history_overscroll ERROR_VARIABLE arbiter_swipe_error GET "${arbiter_engine_manifest}" history_overscroll)
    if(arbiter_swipe_error OR NOT arbiter_history_overscroll)
        message(FATAL_ERROR "Rebuild the current engine patches: this SDK lacks compositor-backed history overscroll.")
    endif()
    string(JSON arbiter_extension_bridge ERROR_VARIABLE arbiter_extension_error GET "${arbiter_engine_manifest}" extension_api_bridge)
    if(arbiter_extension_error OR NOT arbiter_extension_bridge EQUAL 1)
        message(FATAL_ERROR "Rebuild the current engine patches: this SDK lacks Arbiter's native extension API integration.")
    endif()
    # Qt6Config normally searches its own sibling component packages before
    # other prefixes. Select every WebEngine component explicitly so headers,
    # libraries and the helper process come from the same patched SDK.
    foreach(module WebEngineCore WebEngineQuick WebEngineWidgets WebEngineCoreTools)
        unset(arbiter_module_path CACHE)
        find_path(arbiter_module_path "Qt6${module}Config.cmake"
            PATHS "${ARBITER_WEBENGINE_PREFIX}"
            PATH_SUFFIXES "lib/cmake/Qt6${module}" "lib64/cmake/Qt6${module}"
                "lib/${CMAKE_LIBRARY_ARCHITECTURE}/cmake/Qt6${module}"
            NO_DEFAULT_PATH REQUIRED)
        set("Qt6${module}_DIR" "${arbiter_module_path}" CACHE PATH "Arbiter SDK component" FORCE)
    endforeach()
    unset(arbiter_module_path CACHE)
    get_filename_component(ARBITER_ENGINE_LIBDIR "${Qt6WebEngineCore_DIR}/../.." ABSOLUTE)
    if(ARBITER_WEBENGINE_SDK_RUNTIME)
        file(GLOB_RECURSE arbiter_process_candidates
            "${ARBITER_WEBENGINE_PREFIX}/QtWebEngineProcess"
            "${ARBITER_WEBENGINE_PREFIX}/QtWebEngineProcess.exe")
        list(FILTER arbiter_process_candidates EXCLUDE REGEX "\\.dSYM/")
        file(GLOB_RECURSE arbiter_resource_candidates "${ARBITER_WEBENGINE_PREFIX}/qtwebengine_resources.pak")
        file(GLOB_RECURSE arbiter_locale_candidates "${ARBITER_WEBENGINE_PREFIX}/en-US.pak")
        list(FILTER arbiter_locale_candidates INCLUDE REGEX "/qtwebengine_locales/en-US\\.pak$")
        file(GLOB_RECURSE arbiter_qml_candidates "${ARBITER_WEBENGINE_PREFIX}/qmldir")
        list(FILTER arbiter_qml_candidates INCLUDE REGEX "/QtWebEngine/qmldir$")
        foreach(kind process resource locale qml)
            if(NOT arbiter_${kind}_candidates)
                message(FATAL_ERROR "The patched SDK is missing its ${kind} runtime files.")
            endif()
        endforeach()
        list(GET arbiter_process_candidates 0 ARBITER_ENGINE_PROCESS)
        list(GET arbiter_resource_candidates 0 arbiter_resource_file)
        get_filename_component(ARBITER_ENGINE_RESOURCES "${arbiter_resource_file}" DIRECTORY)
        list(GET arbiter_locale_candidates 0 arbiter_locale_file)
        get_filename_component(ARBITER_ENGINE_LOCALES "${arbiter_locale_file}" DIRECTORY)
        list(GET arbiter_qml_candidates 0 arbiter_qml_file)
        get_filename_component(arbiter_qml_module "${arbiter_qml_file}" DIRECTORY)
        get_filename_component(ARBITER_ENGINE_QML "${arbiter_qml_module}" DIRECTORY)
    endif()
endif()

function(arbiter_configure_webengine_runtime target)
    if(ARBITER_PATCHED_WEBENGINE)
        target_compile_definitions(${target} PRIVATE ARBITER_PATCHED_WEBENGINE=1)
        if(APPLE)
            # Base Qt and the SDK both contain WebEngine frameworks. Clang's
            # framework lookup can otherwise select stock headers even though
            # CMake imported the patched component targets correctly.
            target_include_directories(${target} BEFORE PRIVATE
                "${ARBITER_ENGINE_LIBDIR}/QtWebEngineCore.framework")
            target_link_options(${target} BEFORE PRIVATE "-F${ARBITER_ENGINE_LIBDIR}")
        endif()
        if(ARBITER_WEBENGINE_SDK_RUNTIME)
            if(WIN32)
                # Windows resolves imported DLLs before main(), so environment
                # overrides in the runtime initializer would be too late.
                foreach(module WebEngineCore WebEngineQuick WebEngineWidgets)
                    if(TARGET Qt6::${module})
                        add_custom_command(TARGET ${target} POST_BUILD
                            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                "$<TARGET_FILE:Qt6::${module}>" "$<TARGET_FILE_DIR:${target}>"
                            VERBATIM)
                    endif()
                endforeach()
            else()
                set_property(TARGET ${target} APPEND PROPERTY BUILD_RPATH "${ARBITER_ENGINE_LIBDIR}")
            endif()
            target_sources(${target} PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../src/webengineruntime.cpp")
            target_compile_definitions(${target} PRIVATE
                ARBITER_ENGINE_PROCESS="${ARBITER_ENGINE_PROCESS}"
                ARBITER_ENGINE_RESOURCES="${ARBITER_ENGINE_RESOURCES}"
                ARBITER_ENGINE_LOCALES="${ARBITER_ENGINE_LOCALES}"
                ARBITER_WEBENGINE_QML_PATH="${ARBITER_ENGINE_QML}")
        endif()
    endif()
endfunction()
