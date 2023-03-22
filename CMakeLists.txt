set(RGEN_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
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
  RenderHandler.cpp
  sample_code.cpp
  )

BRLCAD_ADDEXEC(rgen "${RGEN_SRCS}"  "libbu;librt;libwdb")

set(rgen_ignore_files
  CMakeLists.txt
  .DS_STORE
  a.out
  README.md
  FactsHandler.h
  IFPainter.h
  InformationGatherer.h
  Options.h
  PerspectiveGatherer.h
  RenderHandler.h
  )
CMAKEFILES(${rgen_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
