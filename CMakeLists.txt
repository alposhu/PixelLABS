# Alternative build for IDEs (Visual Studio "Open Folder", CLion, VS Code).
# build.bat is the primary build and also produces the installer and zip.
cmake_minimum_required(VERSION 3.20)
project(PixelLABS VERSION 1.2.0 LANGUAGES CXX)

if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Pixel LABS builds with 64-bit MSVC on Windows.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)

set(BUNDLE_DIR "${CMAKE_BINARY_DIR}/PixelLABS.vst3/Contents/x86_64-win")

# UI art: the packs in assets/ are not in git (their licence forbids redistribution).
# When they are present, src/gui/UiAssets.inc is baked from them (Python + Pillow + fontTools).
set(UI_ASSETS_INC "${CMAKE_CURRENT_SOURCE_DIR}/src/gui/UiAssets.inc")
set(UI_ASSET_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/UI/DarkAgesUi_v1.0.zip"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/BoldPixels.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/Abaddon_Fonts_v1.2.zip"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/Fibberish v1.0.zip"
)
set(HAVE_UI_ASSETS ON)
foreach(f IN LISTS UI_ASSET_FILES)
    if(NOT EXISTS "${f}")
        set(HAVE_UI_ASSETS OFF)
    endif()
endforeach()
if(HAVE_UI_ASSETS)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_custom_command(
        OUTPUT "${UI_ASSETS_INC}"
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/scripts/make_ui_assets.py"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/make_ui_assets.py" ${UI_ASSET_FILES}
        COMMENT "Baking the UI art into src/gui/UiAssets.inc"
        VERBATIM)
elseif(NOT EXISTS "${UI_ASSETS_INC}")
    message(FATAL_ERROR "The UI art is missing. Download the packs listed in README.md "
                        "(\"Build from source\", \"UI art\") into assets/ and configure again.")
endif()

add_library(PixelLABS SHARED
    "${UI_ASSETS_INC}"
    src/dsp/RetroSynthEngine.cpp
    src/dsp/DemoSongs.cpp
    src/gui/PixelLabsView.cpp
    src/library/SoundLibrary.cpp
    src/plugin/PixelLabsPlugin.cpp
    src/plugin/dllmain.cpp
    src/plugin/vst3.def
)
target_include_directories(PixelLABS PRIVATE include)
target_link_libraries(PixelLABS PRIVATE user32 gdi32)
set_target_properties(PixelLABS PROPERTIES
    PREFIX ""
    OUTPUT_NAME "PixelLABS"
    SUFFIX ".vst3"
    RUNTIME_OUTPUT_DIRECTORY "${BUNDLE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${BUNDLE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${BUNDLE_DIR}"
)

add_executable(test_vst3_host tests/test_vst3_host.cpp src/dsp/DemoSongs.cpp)
target_include_directories(test_vst3_host PRIVATE include)
target_link_libraries(test_vst3_host PRIVATE user32 gdi32)
add_dependencies(test_vst3_host PixelLABS)

enable_testing()
add_test(NAME host_test
         COMMAND test_vst3_host "${BUNDLE_DIR}/PixelLABS.vst3" "${CMAKE_BINARY_DIR}/test-output")
