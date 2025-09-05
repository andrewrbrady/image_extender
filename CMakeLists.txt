cmake_minimum_required(VERSION 3.16)
project(image_extender)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build the wxWidgets app
add_subdirectory(extend_canvas_wx)

# Tools / apps
add_subdirectory(apps/matte_generator)
add_subdirectory(apps/extend_canvas_cli)
