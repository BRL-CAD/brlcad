FIND_PACKAGE(OpenCV)
if(NOT OpenCV_FOUND)
  message("OpenCV is DISABLED")
  return()
endif(NOT OpenCV_FOUND)
message("Found OpenCV: ${OpenCV_LIBS}")

set(RGEN_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )
list(REMOVE_DUPLICATES RGEN_INCLUDE_DIRS)
include_directories(${RGEN_INCLUDE_DIRS})

set(RGEN_SRCS
  FactsHandler.cpp
  IFPainter.cpp
  InformationGatherer.cpp
  main.cpp
  Options.cpp
  PerspectiveGatherer.cpp
  Position.cpp
  RenderHandler.cpp
  )

BRLCAD_ADDEXEC(rgen "${RGEN_SRCS}"  "libbu;librt;libged;${OpenCV_LIBS};")

target_compile_features(rgen PRIVATE cxx_std_17)

set(rgen_ignore_files
  CMakeLists.txt
  FactsHandler.h
  IFPainter.h
  InformationGatherer.h
  Options.h
  PerspectiveGatherer.h
  RenderHandler.h
  Position.h
  picohash.h
  CONTRIBUTING.md
  README.md
  )
CMAKEFILES(${rgen_ignore_files})


MACRO (COPY_DLL trgt libname)
        ADD_CUSTOM_COMMAND (
        TARGET ${trgt}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND}
        ARGS -E copy "${_OpenCV_LIB_PATH}/${libname}.dll" ${CMAKE_CURRENT_BINARY_DIR}
    )
ENDMACRO (COPY_DLL)

COPY_DLL (rgen opencv_world${OpenCV_VERSION_MAJOR}${OpenCV_VERSION_MINOR}${OpenCV_VERSION_PATCH})
COPY_DLL (rgen opencv_world${OpenCV_VERSION_MAJOR}${OpenCV_VERSION_MINOR}${OpenCV_VERSION_PATCH}d)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
