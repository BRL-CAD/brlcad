set(RAW_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  )
list(REMOVE_DUPLICATES RAW_INCLUDE_DIRS)
include_directories(${RAW_INCLUDE_DIRS})

set(rawg_SRCS
  Bot.cpp
  raw-g.cpp
  RegionList.cpp
  )
BRLCAD_ADDEXEC(raw-g "${rawg_SRCS}" "libwdb;librt;libbu" NO_STRICT FOLDER Conv)

BRLCAD_ADDEXEC(g-raw g-raw.c "librt;libgcv;libnmg;libbu;${WINSOCK_LIB}" FOLDER Conv)
add_dependencies(g-raw gcv_plugins)

set(RAW_IGNORE_FILES
  Bot.h
  CMakeLists.txt
  RegionList.h
  )
CMAKEFILES(${RAW_IGNORE_FILES})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
