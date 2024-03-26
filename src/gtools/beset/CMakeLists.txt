set(beset_SRCS
  beset.c
  fitness.c
  population.c
  )

BRLCAD_ADDEXEC(beset "${beset_SRCS}" "libbu;librt;libwdb" NO_INSTALL)
target_include_directories(beset BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

set(beset_ignore_files
  CMakeLists.txt
  beset.h
  fitness.h
  population.h
  )
CMAKEFILES(${beset_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
