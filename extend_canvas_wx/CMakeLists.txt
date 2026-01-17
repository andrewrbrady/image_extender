cmake_minimum_required(VERSION 3.16)
project(extend_canvas_wx)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optional: create a self-contained .app by bundling dylib dependencies
option(ECWX_BUNDLE_DEPS "Bundle Homebrew dylibs into the .app" OFF)

# Find wxWidgets (prefer wx-config; add Homebrew hints on macOS)
if(APPLE AND NOT DEFINED wxWidgets_CONFIG_EXECUTABLE)
    set(_WX_CONFIG_CANDIDATES
        /opt/homebrew/opt/wxwidgets/bin/wx-config
        /usr/local/opt/wxwidgets/bin/wx-config
        /opt/homebrew/bin/wx-config
        /usr/local/bin/wx-config
    )
    foreach(_cand IN LISTS _WX_CONFIG_CANDIDATES)
        if(EXISTS "${_cand}")
            set(wxWidgets_CONFIG_EXECUTABLE "${_cand}")
            message(STATUS "Using wx-config: ${wxWidgets_CONFIG_EXECUTABLE}")
            break()
        endif()
    endforeach()
endif()

find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

# Find OpenCV for the extend_canvas implementation
# Only link the minimal OpenCV components we actually use
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

# Sources for the wxWidgets UI
set(SRC
    src/wxMain.cpp
    src/WxMainFrame.cpp
    src/WxControlPanel.cpp
    src/WxPreviewPanel.cpp
)

set(HDR
    src/WxMainFrame.hpp
    src/WxControlPanel.hpp
    src/WxPreviewPanel.hpp
)

add_executable(${PROJECT_NAME} MACOSX_BUNDLE
    ${SRC}
    ${HDR}
    # Reuse the shared extend_canvas implementation (moved to shared/)
    ../shared/extend_canvas/extend_canvas.cpp
    ../shared/extend_canvas/extend_canvas.hpp
    # Auto Fit Vehicle implementation
    ../shared/auto_fit_vehicle/auto_fit_vehicle.cpp
    ../shared/auto_fit_vehicle/auto_fit_vehicle.hpp
    # Vehicle mask support
    ../shared/vehicle_mask/vehicle_mask.cpp
    ../shared/vehicle_mask/vehicle_mask.hpp
    # Shared utility implementations
    ../shared/util/ImageOps.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/extend_canvas
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/auto_fit_vehicle
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/vehicle_mask
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${wxWidgets_LIBRARIES}
    ${OpenCV_LIBS}
)

# macOS bundle metadata (optional)
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.motive.extendcanvas.wx"
        MACOSX_BUNDLE_BUNDLE_NAME "ExtendCanvasWX"
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
    if(ECWX_BUNDLE_DEPS)
        # Post-build: bundle local OpenCV and key deps into the .app and sign
        set(_APP_BUNDLE "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>")
        # Derive the OpenCV libdir from OpenCV_DIR (points to .../lib/cmake/opencv4)
        if(DEFINED OpenCV_DIR)
            get_filename_component(_OPENCV_CMAKE_DIR "${OpenCV_DIR}" ABSOLUTE)
            get_filename_component(_OPENCV_LIBDIR "${_OPENCV_CMAKE_DIR}/../.." ABSOLUTE)
        endif()
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -DAPP_BUNDLE=${_APP_BUNDLE}
                -DOPENCV_LIBDIR=${_OPENCV_LIBDIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/BundleLocalOpenCV.cmake
            COMMAND /usr/bin/codesign --force --deep --sign - ${_APP_BUNDLE}
            VERBATIM
        )
    endif()
endif()
