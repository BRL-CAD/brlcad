set(RAW_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  )
LIST(REMOVE_DUPLICATES RAW_INCLUDE_DIRS)
include_directories(${RAW_INCLUDE_DIRS})

if (MSVC)
  add_definitions(
    -DON_DLL_IMPORTS
    )
endif (MSVC)

set(rawg_SRCS
  Bot.cpp
  raw-g.cpp
  RegionList.cpp
  )
BRLCAD_ADDEXEC(raw-g "${rawg_SRCS}" "libwdb;librt;libbu" NO_STRICT)

BRLCAD_ADDEXEC(g-raw g-raw.c "librt;libgcv;libbu;${WINSOCK_LIB}")

set(RAW_IGNORE_FILES
  Bot.h
  RegionList.h
  )
CMAKEFILES(${RAW_IGNORE_FILES})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
