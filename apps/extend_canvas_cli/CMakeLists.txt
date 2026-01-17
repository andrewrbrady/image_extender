cmake_minimum_required(VERSION 3.16)
project(extend_canvas_cli)

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

add_executable(extend_canvas_cli
    extend_canvas_cli.cpp
)

target_link_libraries(extend_canvas_cli PRIVATE ${OpenCV_LIBS})
