set(VISUALIZATION_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  ${TCLCAD_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${PKG_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )
list(REMOVE_DUPLICATES VISUALIZATION_INCLUDE_DIRS)
include_directories(${VISUALIZATION_INCLUDE_DIRS})

set(VISUALIZATION_SRCS
  FactsHandler.cpp
  IFPainter.cpp
  InformationGatherer.cpp
  main.cpp
  Options.cpp
  PerspectiveGatherer.cpp
  RenderHandler.cpp
  sample_code.cpp
  )

BRLCAD_ADDEXEC(visualization "${VISUALIZATION_SRCS}"  "libbu;librt;libwdb")

set(visualization_ignore_files
  CMakeLists.txt
  .DS_STORE
  .git
  a.out
  FactsHandler.h
  IFPainter.h
  InformationGatherer.h
  Options.h
  PerspectiveGatherer.h
  RenderHandler.h
  )
CMAKEFILES(${visualization_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
