# Include directories needed by libdm users
set(DM_NULL_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  )
include_directories(${DM_NULL_INCLUDE_DIRS})

set(NULL_SRCS
  dm-Null.c
  if_null.c
  )

add_library(dm_null SHARED ${NULL_SRCS})
target_link_libraries(dm_null libdm libbu)
if (CPP_DLL_DEFINES)
  set_property(TARGET dm_null APPEND PROPERTY COMPILE_DEFINITIONS "DM_DLL_EXPORTS")
endif (CPP_DLL_DEFINES)
VALIDATE_STYLE(dm_null "${NULL_SRCS}")

PLUGIN_SETUP(dm_null dm)

CMAKEFILES(dm-Null.h)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
