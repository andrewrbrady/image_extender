cmake_minimum_required(VERSION 3.16)
project(extend_canvas_ui)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt packages
find_package(Qt6 COMPONENTS
    Core
    Gui
    Widgets
    REQUIRED
)

# Find OpenCV
find_package(OpenCV REQUIRED)

# Add extend_canvas library
add_library(extend_canvas STATIC
    src/extend_canvas.cpp
    src/extend_canvas.hpp
)

# Link OpenCV to extend_canvas
target_link_libraries(extend_canvas PRIVATE
    ${OpenCV_LIBS}
)

# Set up main application sources
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/widgets/BatchListWidget.cpp
    src/widgets/DropLabel.cpp
    src/panels/ControlPanel.cpp
    src/panels/PreviewPanel.cpp
)

set(HEADERS
    src/MainWindow.hpp
    src/widgets/BatchListWidget.hpp
    src/widgets/DropLabel.hpp
    src/panels/ControlPanel.hpp
    src/panels/PreviewPanel.hpp
    src/models/ImageSettings.hpp
)

# Create main executable
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    extend_canvas
    ${OpenCV_LIBS}
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
)

# Set macOS bundle properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.motive.extendcanvas"
    MACOSX_BUNDLE_BUNDLE_NAME "ExtendCanvas"
)
