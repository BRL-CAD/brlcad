set(AUX_SRCS
  cc.c
  cpuinfo.c
  mmatomic.c
  mmbinsort.c
  mmbitmap.c
  mm.c
  mmhash.c
  )

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(AUX SHARED ${AUX_SRCS})
set_target_properties(AUX PROPERTIES VERSION 1.1.0 SOVERSION 1)

install(TARGETS AUX
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if(MSVC)
  set_property(TARGET AUX APPEND PROPERTY COMPILE_DEFINITIONS "AUX_DLL_EXPORTS")
endif(MSVC)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
