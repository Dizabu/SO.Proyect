cmake_minimum_required(VERSION 3.20)
project(untitled C)

set(CMAKE_C_STANDARD 11)

find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

include_directories(
        ${SDL2_INCLUDE_DIRS}
        ${SDL2_TTF_INCLUDE_DIRS}
)

add_executable(untitled
        main.c
        bridge.c
        vehicle.c
        traffic_generator.c
        config.c
        gui.c
)

target_link_libraries(untitled
        ${SDL2_LIBRARIES}
        SDL2_ttf
        pthread
        m
)