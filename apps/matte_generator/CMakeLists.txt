cmake_minimum_required(VERSION 3.16)
project(matte_generator)

find_package(OpenCV REQUIRED)

add_executable(matte_generator
    matte_generator.cpp
)

target_link_libraries(matte_generator PRIVATE ${OpenCV_LIBS})

