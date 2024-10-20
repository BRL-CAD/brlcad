# -*- cmake -*-
# - Find TinyGLTF

# TinyGLTF_INCLUDE_DIR      TinyGLTF's include directory

find_package(PackageHandleStandardArgs)

set(TinyGLTF_INCLUDE_DIR "${TinyGLTF_DIR}/../include" CACHE STRING "TinyGLTF include directory")

find_file(TinyGLTF_HEADER tiny_gltf.h PATHS ${TinyGLTF_INCLUDE_DIR})

if(NOT TinyGLTF_HEADER)
  message(FATAL_ERROR "Unable to find tiny_gltf.h, TinyGLTF_INCLUDE_DIR = ${TinyGLTF_INCLUDE_DIR}")
endif()
