cmake_minimum_required(VERSION 3.16)
project(extend_canvas_wx)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
find_package(OpenCV REQUIRED)

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
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/extend_canvas
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
    )
endif()
