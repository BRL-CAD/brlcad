cmake_minimum_required(VERSION 3.1)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type")
    message(STATUS "No build type selected, default to ${CMAKE_BUILD_TYPE}")
endif()

project(AMGCL)

if (NOT (CMAKE_VERSION LESS 3.3))
    cmake_policy(SET CMP0058 OLD)
endif()

set(AMGCL_MASTER_PROJECT OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(AMGCL_MASTER_PROJECT ON)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
if (CMAKE_VERSION VERSION_LESS "3.1.0")
    set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/opencl)
endif()

#----------------------------------------------------------------------------
# Builtin backend
#----------------------------------------------------------------------------
add_library(amgcl INTERFACE)
add_library(amgcl::amgcl ALIAS amgcl)

target_compile_features(amgcl INTERFACE
    cxx_auto_type
    cxx_range_for
    cxx_rvalue_references
    cxx_right_angle_brackets
    cxx_static_assert
)

target_include_directories(amgcl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    )

#----------------------------------------------------------------------------
# Common compile options and definitions
#----------------------------------------------------------------------------
target_compile_options(amgcl INTERFACE
    # Compiler is GNU (g++):
    $<$<CXX_COMPILER_ID:GNU>:$<BUILD_INTERFACE:-Wall;-Wextra;-Wpedantic>>
    # Compiler is Clang:
    $<$<CXX_COMPILER_ID:Clang>:$<BUILD_INTERFACE:-Wall;-Wextra;-Wpedantic;-Wno-c99-extensions>>
    # Compiler is MSVC:
    $<$<CXX_COMPILER_ID:MSVC>:/bigobj>
    $<$<CXX_COMPILER_ID:MSVC>:/wd4715>
    )

target_compile_definitions(amgcl INTERFACE
    # Compiler is MSVC:
    $<$<CXX_COMPILER_ID:MSVC>:NOMINMAX>
    $<$<CXX_COMPILER_ID:MSVC>:_USE_MATH_DEFINES>
    $<$<CXX_COMPILER_ID:MSVC>:_VARIADIC_MAX=10>
    $<$<CXX_COMPILER_ID:MSVC>:_SCL_SECURE_NO_WARNINGS>
    )

#----------------------------------------------------------------------------
# Eigen backend
#----------------------------------------------------------------------------
find_path(EIGEN_INCLUDE Eigen/SparseCore PATH_SUFFIXES eigen3)
if (EIGEN_INCLUDE)
    add_library(eigen_target INTERFACE)
    target_include_directories(eigen_target INTERFACE ${EIGEN_INCLUDE})
    target_compile_options(eigen_target INTERFACE
        $<$<CXX_COMPILER_ID:GNU>:-Wno-int-in-bool-context>
        $<$<CXX_COMPILER_ID:GNU>:-Wno-maybe-uninitialized>
        $<$<CXX_COMPILER_ID:GNU>:-Wno-deprecated-copy>
        $<$<CXX_COMPILER_ID:Clang>:-Wno-deprecated-copy>
        $<$<CXX_COMPILER_ID:Clang>:-Wno-c++11-long-long>
        )
    target_compile_definitions(eigen_target INTERFACE AMGCL_HAVE_EIGEN)
endif()


