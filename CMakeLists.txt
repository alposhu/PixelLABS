# Alternative build for IDEs (Visual Studio "Open Folder", CLion, VS Code).
# build.bat is the primary build and also produces the installer and zip.
cmake_minimum_required(VERSION 3.20)
project(PixelLABS VERSION 2.0.0 LANGUAGES CXX)

if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Pixel LABS builds with 64-bit MSVC on Windows.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)

set(BUNDLE_DIR "${CMAKE_BINARY_DIR}/PixelLABS.vst3/Contents/x86_64-win")

add_library(PixelLABS SHARED
    src/dsp/RetroSynthEngine.cpp
    src/gui/PixelLabsView.cpp
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

add_executable(test_vst3_host tests/test_vst3_host.cpp)
target_include_directories(test_vst3_host PRIVATE include)
target_link_libraries(test_vst3_host PRIVATE user32 gdi32)
add_dependencies(test_vst3_host PixelLABS)

enable_testing()
add_test(NAME host_test
         COMMAND test_vst3_host "${BUNDLE_DIR}/PixelLABS.vst3" "${CMAKE_BINARY_DIR}/test-output")
