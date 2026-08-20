cmake_minimum_required(VERSION 3.12)
project(brlcad_probe_validation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(brlcad_probe_validation probe_validation.cpp)

