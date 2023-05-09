set(RGEN_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
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

BRLCAD_ADDEXEC(rgen "${RGEN_SRCS}"  "libbu;librt;libged;")

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
  report.png
  )
CMAKEFILES(${rgen_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
