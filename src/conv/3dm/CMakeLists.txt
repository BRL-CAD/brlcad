if(MSVC)
  add_definitions(-DON_DLL_IMPORTS)
endif(MSVC)

set(3DM_INCLUDE_DIRS
  ${OPENNURBS_INCLUDE_DIRS}
  ${ICV_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(3DM_INCLUDE_DIRS)

set(3dm-g_SRCS
  conv3dm-g.cpp
  3dm-g.cpp
  )
BRLCAD_ADDEXEC(3dm-g "${3dm-g_SRCS}" "libwdb;libicv;${OPENNURBS_LIBRARY}" NO_STRICT)

CMAKEFILES(
  conv3dm-g.hpp
  solidity.cpp
  solidity.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
