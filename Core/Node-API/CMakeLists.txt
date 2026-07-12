# Set per-platform defaults if unspecified.
if(WIN32)
    set(NAPI_JAVASCRIPT_ENGINE "Chakra" CACHE STRING "JavaScript engine for Node-API")
elseif(APPLE)
    set(NAPI_JAVASCRIPT_ENGINE "JavaScriptCore" CACHE STRING "JavaScript engine for Node-API")
elseif(ANDROID)
    set(NAPI_JAVASCRIPT_ENGINE "V8" CACHE STRING "JavaScript engine for Node-API")
elseif(UNIX)
    set(NAPI_JAVASCRIPT_ENGINE "JavaScriptCore" CACHE STRING "JavaScript engine for Node-API")
    find_library(JAVASCRIPTCORE_LIBRARY javascriptcoregtk-4.1)
    if(NOT JAVASCRIPTCORE_LIBRARY)
        message(FATAL_ERROR "JavaScriptCore library not found. Please install libwebkit2gtk-4.1-dev")
    endif()
else()
    message(FATAL_ERROR "Unable to select Node-API JavaScript engine for platform")
endif()

set(SOURCES
    "Include/Engine/${NAPI_JAVASCRIPT_ENGINE}/napi/env.h"
    "Include/Shared/napi/js_native_api.h"
    "Include/Shared/napi/js_native_api_types.h"
    "Include/Shared/napi/napi.h"
    "Include/Shared/napi/napi-inl.h"
    "Source/env.cc")

set(INCLUDE_DIRECTORIES
    PUBLIC "Include/Shared"
    PUBLIC "Include/Engine/${NAPI_JAVASCRIPT_ENGINE}")

if(NAPI_BUILD_ABI)
    if(ANDROID)
        function(napi_install_android_package name aar_path output_directory)
            file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/package-${name}.json" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
            file(RENAME "${CMAKE_CURRENT_BINARY_DIR}/package-${name}.json" "${CMAKE_CURRENT_BINARY_DIR}/package.json")
            npm(install --no-package-lock --silent WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

            file(GLOB_RECURSE ANDROID_ARCHIVE "${CMAKE_CURRENT_BINARY_DIR}/node_modules/${V8_PACKAGE_NAME}/${aar_path}/*.aar")
            file(ARCHIVE_EXTRACT INPUT ${ANDROID_ARCHIVE} DESTINATION ${output_directory} PATTERNS jni)
            message(STATUS "Extracting ${V8_PACKAGE_NAME} archive - done")

            file(COPY "${CMAKE_CURRENT_BINARY_DIR}/node_modules/${V8_PACKAGE_NAME}/dist/include" DESTINATION ${output_directory})
        endfunction()
    endif()

    if(NAPI_JAVASCRIPT_ENGINE STREQUAL "Chakra")
        set(SOURCES ${SOURCES}
            "Source/env_chakra.cc"
            "Source/js_native_api_chakra.cc"
            "Source/js_native_api_chakra.h")

        set(LINK_LIBRARIES ${LINK_LIBRARIES}
            INTERFACE "chakrart.lib")
    elseif(NAPI_JAVASCRIPT_ENGINE STREQUAL "JavaScriptCore")
        set(SOURCES ${SOURCES}
            "Source/env_javascriptcore.cc"
            "Source/js_native_api_javascriptcore.cc"
            "Source/js_native_api_javascriptcore.h")

        if(ANDROID)
            # Bun publishes the JavaScriptCore build used by each release as pinned Android static
            # archives. Relink the selected archives into the libjsc.so boundary expected by napi,
            # instead of installing an npm package only to unpack its embedded AAR.
            set(JSR_JSC_BUN_WEBKIT_REVISION "5488984d20e0dbfe4be2c3ba8fb18eb81a5e0e8b")
            set(JSR_JSC_BUN_WEBKIT_RELEASE "autobuild-${JSR_JSC_BUN_WEBKIT_REVISION}")
            set(JSR_JSC_ANDROID_ROOT "" CACHE PATH
                "Path to an extracted Bun Android WebKit 'bun-webkit' directory; empty downloads the pinned release")

            string(REGEX REPLACE "^android-" "" JSR_JSC_ANDROID_API_LEVEL "${ANDROID_PLATFORM}")
            if(JSR_JSC_ANDROID_API_LEVEL LESS 28)
                message(FATAL_ERROR
                    "Bun's pinned Android WebKit archives target API 28; set Android minSdkVersion to 28 or newer")
            endif()

            if(ANDROID_ABI STREQUAL "arm64-v8a")
                set(JSR_JSC_BUN_WEBKIT_ASSET "bun-webkit-linux-arm64-android.tar.gz")
                set(JSR_JSC_BUN_WEBKIT_SHA256 "63c18a18c89dc3e1c0a93107f95290c27fbe310eb90e30dc197a7936804d034b")
            elseif(ANDROID_ABI STREQUAL "x86_64")
                set(JSR_JSC_BUN_WEBKIT_ASSET "bun-webkit-linux-amd64-android.tar.gz")
                set(JSR_JSC_BUN_WEBKIT_SHA256 "c198efb68c17ae65ff66968f7d04bdab4845a5abaefb941b15e0e2b36aaab774")
            else()
                message(FATAL_ERROR
                    "Bun Android JavaScriptCore supports only arm64-v8a and x86_64; got '${ANDROID_ABI}'")
            endif()

            if(JSR_JSC_ANDROID_ROOT)
                set(JSC_ANDROID_DIR "${JSR_JSC_ANDROID_ROOT}")
            else()
                FetchContent_Declare(bun_webkit_android
                    URL
                        "https://github.com/oven-sh/WebKit/releases/download/${JSR_JSC_BUN_WEBKIT_RELEASE}/${JSR_JSC_BUN_WEBKIT_ASSET}"
                    URL_HASH "SHA256=${JSR_JSC_BUN_WEBKIT_SHA256}")
                FetchContent_GetProperties(bun_webkit_android)
                if(NOT bun_webkit_android_POPULATED)
                    message(STATUS
                        "Downloading Bun WebKit ${JSR_JSC_BUN_WEBKIT_REVISION} for ${ANDROID_ABI}")
                    FetchContent_Populate(bun_webkit_android)
                endif()
                # FetchContent strips the archive's single top-level bun-webkit directory.
                set(JSC_ANDROID_DIR "${bun_webkit_android_SOURCE_DIR}")
            endif()

            set(JSC_ANDROID_STATIC_LIBRARIES
                "${JSC_ANDROID_DIR}/lib/libJavaScriptCore.a"
                "${JSC_ANDROID_DIR}/lib/libWTF.a"
                "${JSC_ANDROID_DIR}/lib/libbmalloc.a"
                "${JSC_ANDROID_DIR}/lib/libicui18n.a"
                "${JSC_ANDROID_DIR}/lib/libicuuc.a"
                "${JSC_ANDROID_DIR}/lib/libicudata.a")

            foreach(JSC_ANDROID_FILE
                    "${JSC_ANDROID_DIR}/include/JavaScriptCore/JavaScript.h"
                    ${JSC_ANDROID_STATIC_LIBRARIES})
                if(NOT EXISTS "${JSC_ANDROID_FILE}")
                    message(FATAL_ERROR "Bun Android WebKit archive is missing '${JSC_ANDROID_FILE}'")
                endif()
            endforeach()

            set(INCLUDE_DIRECTORIES ${INCLUDE_DIRECTORIES}
                PUBLIC "${JSC_ANDROID_DIR}/include")

            add_library(jsc_android SHARED "Source/jsc_android.cpp")
            set_target_properties(jsc_android PROPERTIES
                OUTPUT_NAME jsc
                CXX_STANDARD 23
                CXX_STANDARD_REQUIRED ON
                LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/jsc_android.exports")
            target_compile_definitions(jsc_android PRIVATE
                JSR_JSC_BUN_WEBKIT_REVISION="${JSR_JSC_BUN_WEBKIT_REVISION}"
                BUILDING_JSCONLY__
                BUILDING_JavaScriptCore
                BUILDING_WEBKIT=1
                BUILDING_WITH_CMAKE=1
                HAVE_CONFIG_H=1
                PAS_BMALLOC=1
                STATICALLY_LINKED_WITH_WTF
                STATICALLY_LINKED_WITH_bmalloc
                U_STATIC_IMPLEMENTATION=1)
            target_include_directories(jsc_android PUBLIC "${JSC_ANDROID_DIR}/include")
            target_link_options(jsc_android PRIVATE
                "-Wl,--no-undefined"
                "-Wl,-Bsymbolic"
                "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/Source/jsc_android.exports"
                "-Wl,-z,max-page-size=16384"
                "-Wl,-z,common-page-size=16384")
            target_link_libraries(jsc_android PRIVATE
                "-Wl,--whole-archive"
                "${JSC_ANDROID_DIR}/lib/libJavaScriptCore.a"
                "-Wl,--no-whole-archive"
                "-Wl,--start-group"
                "${JSC_ANDROID_DIR}/lib/libWTF.a"
                "${JSC_ANDROID_DIR}/lib/libbmalloc.a"
                "${JSC_ANDROID_DIR}/lib/libicui18n.a"
                "${JSC_ANDROID_DIR}/lib/libicuuc.a"
                "${JSC_ANDROID_DIR}/lib/libicudata.a"
                "-Wl,--end-group"
                log
                dl
                m)
            set_property(TARGET jsc_android PROPERTY FOLDER Dependencies)

            set(LINK_LIBRARIES ${LINK_LIBRARIES}
                PUBLIC jsc_android)
        elseif(APPLE)
            find_library(JAVASCRIPTCORE_LIBRARY JavaScriptCore)
            set(LINK_LIBRARIES ${LINK_LIBRARIES}
                PUBLIC ${JAVASCRIPTCORE_LIBRARY})
        elseif(UNIX)
            set(LINK_LIBRARIES ${LINK_LIBRARIES}
                PUBLIC ${JAVASCRIPTCORE_LIBRARY})
            set(INCLUDE_DIRECTORIES ${INCLUDE_DIRECTORIES}
                PUBLIC "/usr/include/webkitgtk-4.1/")
        else()
            message(FATAL_ERROR "Unsupported JavaScript engine: ${NAPI_JAVASCRIPT_ENGINE}")
        endif()
    elseif(NAPI_JAVASCRIPT_ENGINE STREQUAL "V8")
        set(SOURCES ${SOURCES}
            "Source/env_v8.cc"
            "Source/js_native_api_v8.cc"
            "Source/js_native_api_v8.h"
            "Source/js_native_api_v8_internals.h")

        if(ANDROID)
            set(V8_PACKAGE_BASE_NAME "v8-android-jit-nointl-nosnapshot")
            set(V8_PACKAGE_NAME "@babylonjs/${V8_PACKAGE_BASE_NAME}")
            set(V8_ANDROID_DIR "${CMAKE_CURRENT_BINARY_DIR}/${V8_PACKAGE_BASE_NAME}")
            napi_install_android_package(v8 "dist/org/chromium" ${V8_ANDROID_DIR})

            set(INCLUDE_DIRECTORIES ${INCLUDE_DIRECTORIES}
                PUBLIC "${V8_ANDROID_DIR}/include")

            set(LINK_LIBRARIES ${LINK_LIBRARIES}
                PUBLIC "${V8_ANDROID_DIR}/jni/${ANDROID_ABI}/libv8android.so")
        elseif(WIN32)
            set_cpu_platform_arch()
            set(V8_VERSION "11.9.169.4")
            download_nuget()
            set(V8_PACKAGE_PATH "${NUGET_PATH}/packages/v8-v143-${CPU_ARCH}.${V8_VERSION}")
            set(V8_PACKAGE_PATH "${NUGET_PATH}/packages/v8-v143-${CPU_ARCH}.${V8_VERSION}")
            set(V8_REDIST_PACKAGE_PATH "${NUGET_PATH}/packages/v8.redist-v143-${CPU_ARCH}.${V8_VERSION}")

            add_library(v8_libbase SHARED IMPORTED)
            set_target_properties(v8_libbase PROPERTIES IMPORTED_IMPLIB "${V8_PACKAGE_PATH}/lib/Release/v8_libbase.dll.lib")
            add_library(v8_libplatform SHARED IMPORTED)
            set_target_properties(v8_libplatform PROPERTIES IMPORTED_IMPLIB "${V8_PACKAGE_PATH}/lib/Release/v8_libplatform.dll.lib")
            add_library(v8 SHARED IMPORTED)
            set_target_properties(v8 PROPERTIES IMPORTED_IMPLIB "${V8_PACKAGE_PATH}/lib/Release/v8.dll.lib")
            target_link_libraries(v8 INTERFACE v8_libbase INTERFACE v8_libplatform)
            target_include_directories(v8 INTERFACE "${V8_PACKAGE_PATH}/include")

            set(V8_DIST
                "${V8_REDIST_PACKAGE_PATH}/lib/Release/icudtl.dat"
                "${V8_REDIST_PACKAGE_PATH}/lib/Release/third_party_icu_icui18n.dll"
                "${V8_REDIST_PACKAGE_PATH}/lib/Release/third_party_abseil-cpp_absl.dll"
                "${V8_REDIST_PACKAGE_PATH}/lib/Release/icuuc.dll"
                "${V8_REDIST_PACKAGE_PATH}/lib/Release/v8.dll"
                "${V8_REDIST_PACKAGE_PATH}/lib/Release/v8_libbase.dll"
                "${V8_REDIST_PACKAGE_PATH}/lib/Release/v8_libplatform.dll"
                "${V8_REDIST_PACKAGE_PATH}/lib/Release/third_party_zlib.dll")

            # only 1 imported location per library -> Adding 1 library per file
            foreach(V8FILE ${V8_DIST})
                get_filename_component(V8FILE_NAME "${V8FILE}" NAME_WE)
                add_library("v8::${V8FILE_NAME}" SHARED IMPORTED)
                set_target_properties("v8::${V8FILE_NAME}" PROPERTIES IMPORTED_IMPLIB "${V8_PACKAGE_PATH}/lib/Release/v8_libbase.dll.lib")
                set_target_properties("v8::${V8FILE_NAME}" PROPERTIES IMPORTED_LOCATION ${V8FILE})
            endforeach()

            set(LINK_LIBRARIES ${LINK_LIBRARIES}
                PUBLIC v8
                PRIVATE v8::icudtl
                PRIVATE v8::third_party_icu_icui18n
                PRIVATE v8::icuuc
                PRIVATE v8::v8
                PRIVATE v8::v8_libbase
                PRIVATE v8::v8_libplatform
                PRIVATE v8::third_party_zlib)
        else()
            message(FATAL_ERROR "Unsupported JavaScript engine: ${NAPI_JAVASCRIPT_ENGINE}")
        endif()
    else()
        message(FATAL_ERROR "Unsupported JavaScript engine: ${NAPI_JAVASCRIPT_ENGINE}")
    endif()

    message(STATUS "Selected ${NAPI_JAVASCRIPT_ENGINE}")
endif()

# On Android, native addons are dlopen'd as standalone .node modules and resolve their napi_* imports
# from a shared napi at load time -- bionic will not surface a statically-linked host's napi to a
# dlopen'd module, so the host and every addon must share a single libnapi.so. Default napi to a
# shared library on Android so that model works out of the box; an integrator who wants a static napi
# (e.g. for size/packaging) can override with -DJSR_NAPI_SHARED=OFF. The option defaults OFF on other
# platforms, where napi keeps following the project's default library type (i.e. honors
# BUILD_SHARED_LIBS).
set(JSR_NAPI_SHARED_DEFAULT OFF)
if(ANDROID)
    set(JSR_NAPI_SHARED_DEFAULT ON)
endif()
option(JSR_NAPI_SHARED "Build napi as a shared library (libnapi.so)" ${JSR_NAPI_SHARED_DEFAULT})

if(JSR_NAPI_SHARED)
    add_library(napi SHARED ${SOURCES})
else()
    add_library(napi ${SOURCES})
endif()

target_include_directories(napi ${INCLUDE_DIRECTORIES})
target_link_libraries(napi ${LINK_LIBRARIES})

# Expose the selected engine as a compile definition so engine-agnostic consumers (the node_lite test
# harness) can branch on engine capability instead of guessing from the platform
# (__APPLE__ == JSC / __ANDROID__ == V8 breaks Android-JSC, Linux-V8, Windows-Chakra, ...).
string(TOUPPER "${NAPI_JAVASCRIPT_ENGINE}" NAPI_ENGINE_UPPER)
target_compile_definitions(napi PUBLIC JSR_NAPI_ENGINE_${NAPI_ENGINE_UPPER})

set_property(TARGET napi PROPERTY FOLDER Dependencies)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
