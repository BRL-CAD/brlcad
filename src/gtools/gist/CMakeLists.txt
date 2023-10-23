set(gist_ignore_files
  CMakeLists.txt
  CONTRIBUTING.md
  FactsHandler.h
  IFPainter.h
  InformationGatherer.h
  LICENSE
  Options.h
  PerspectiveGatherer.h
  Position.h
  README.md
  RenderHandler.h
  TODO
  pch.h
  picohash.h
)

set(GIST_SRCS
  FactsHandler.cpp
  IFPainter.cpp
  InformationGatherer.cpp
  main.cpp
  Options.cpp
  PerspectiveGatherer.cpp
  Position.cpp
  RenderHandler.cpp
)

CMAKEFILES(${gist_ignore_files} ${GIST_SRCS})

FIND_PACKAGE(OpenCV)
if(NOT OpenCV_FOUND)
  message("OpenCV is DISABLED")
  return()
endif(NOT OpenCV_FOUND)
message("Found OpenCV: ${OpenCV_LIBS}")

set(GIST_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
)
list(REMOVE_DUPLICATES GIST_INCLUDE_DIRS)
include_directories(${GIST_INCLUDE_DIRS})

BRLCAD_ADDEXEC(gist "${GIST_SRCS}"  "libbu;librt;libged;${OpenCV_LIBS};")

target_compile_features(gist PRIVATE cxx_std_17)



MACRO (COPY_DLL trgt libname)
  ADD_CUSTOM_COMMAND (
    TARGET ${trgt}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    ARGS -E copy_if_different "${_OpenCV_LIB_PATH}/${libname}.dll" ${CMAKE_CURRENT_BINARY_DIR}
  )
ENDMACRO (COPY_DLL)

if (CMAKE_SHARED_LIBRARY_SUFFIX STREQUAL ".dll")
  COPY_DLL(gist opencv_world${OpenCV_VERSION_MAJOR}${OpenCV_VERSION_MINOR}${OpenCV_VERSION_PATCH})
  COPY_DLL(gist opencv_world${OpenCV_VERSION_MAJOR}${OpenCV_VERSION_MINOR}${OpenCV_VERSION_PATCH}d)
endif (CMAKE_SHARED_LIBRARY_SUFFIX STREQUAL ".dll")


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
