cmake_minimum_required(VERSION 3.15)

project(win-theme-shift)

add_executable(win-theme-shift main.cpp)

target_compile_features(win-theme-shift PRIVATE cxx_std_17)

target_link_libraries(win-theme-shift PRIVATE advapi32 user32)
