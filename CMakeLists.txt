cmake_minimum_required(VERSION 2.8)

project(keycode2midi)
add_executable(${PROJECT_NAME} "main.c")

target_link_libraries(${PROJECT_NAME} "asound")
target_link_libraries(${PROJECT_NAME} "confuse")