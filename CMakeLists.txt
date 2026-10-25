cmake_minimum_required(VERSION 3.14.0)

option(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES "Enable VST 3 Plug-in Examples" OFF)
option(SMTG_ENABLE_VST3_HOSTING_EXAMPLES "Enable VST 3 Hosting Examples" OFF)

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.13 CACHE STRING "")

# Path to the VST3 SDK: pass -DVST3_SDK_ROOT=<path> or set the VST3_SDK_ROOT environment variable.
if(NOT VST3_SDK_ROOT AND DEFINED ENV{VST3_SDK_ROOT})
    file(TO_CMAKE_PATH "$ENV{VST3_SDK_ROOT}" VST3_SDK_ROOT)
endif()
set(VST3_SDK_ROOT "${VST3_SDK_ROOT}" CACHE PATH "Path to the VST3 SDK")

set(vst3sdk_SOURCE_DIR "${VST3_SDK_ROOT}")
if(NOT vst3sdk_SOURCE_DIR)
    message(FATAL_ERROR "Path to VST3 SDK is empty! Set the VST3_SDK_ROOT environment variable or pass -DVST3_SDK_ROOT=<path>.")
endif()
if(NOT EXISTS "${vst3sdk_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "No VST3 SDK found at '${vst3sdk_SOURCE_DIR}'. Check VST3_SDK_ROOT.")
endif()

project(Wavegain
    # This is your plug-in version number. Change it here only.
    # Version number symbols usable in C++ can be found in
    # source/version.h and ${PROJECT_BINARY_DIR}/projectversion.h.
    VERSION 1.0.0.0 
    DESCRIPTION "Wavegain VST 3 Plug-in"
)

set(SMTG_VSTGUI_ROOT "${vst3sdk_SOURCE_DIR}")

add_subdirectory(${vst3sdk_SOURCE_DIR} ${PROJECT_BINARY_DIR}/vst3sdk)
smtg_enable_vst3_sdk()

smtg_add_vst3plugin(Wavegain
    source/version.h
    source/cids.h
    source/wavegain.h
    source/wavegain.cpp
    source/entry.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/vstsinglecomponenteffect.h
    ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/vstsinglecomponenteffect.cpp
)

#- VSTGUI Wanted ----
if(SMTG_ENABLE_VSTGUI_SUPPORT)
    target_sources(Wavegain
        PRIVATE
            resource/editor.uidesc
    )
    target_link_libraries(Wavegain
        PRIVATE
            vstgui_support
    )
    smtg_target_add_plugin_resources(Wavegain
        RESOURCES
            "resource/editor.uidesc"
    )
endif(SMTG_ENABLE_VSTGUI_SUPPORT)
# -------------------

smtg_target_add_plugin_snapshots (Wavegain
    RESOURCES
        resource/4566F4A245D657CA9913E9CEF5259CFD_snapshot.png
        resource/4566F4A245D657CA9913E9CEF5259CFD_snapshot_2.0x.png
)

target_link_libraries(Wavegain
    PRIVATE
        sdk
)

smtg_target_configure_version_file(Wavegain)

if(SMTG_MAC)
    smtg_target_set_bundle(Wavegain
        BUNDLE_IDENTIFIER airflame.wavegain
        COMPANY_NAME "Airflame"
    )
    smtg_target_set_debug_executable(Wavegain
        "/Applications/VST3PluginTestHost.app"
        "--pluginfolder;$(BUILT_PRODUCTS_DIR)"
    )
elseif(SMTG_WIN)
    target_sources(Wavegain PRIVATE 
        resource/win32resource.rc
    )
    if(MSVC)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Wavegain)

        smtg_target_set_debug_executable(Wavegain
            "$(ProgramW6432)/Steinberg/VST3PluginTestHost/VST3PluginTestHost.exe"
            "--pluginfolder \"$(OutDir)/\""
        )
    endif()
endif(SMTG_MAC)
