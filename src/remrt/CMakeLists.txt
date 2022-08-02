
if (NOT WIN32)
  set(REMRTBIN_INCLUDE_DIRS
    ${REMRT_INCLUDE_DIRS}
    ${OPTICAL_INCLUDE_DIRS}
    ${FB_INCLUDE_DIRS}
    ${PKG_INCLUDE_DIRS}
    )
  LIST(REMOVE_DUPLICATES REMRTBIN_INCLUDE_DIRS)
  BRLCAD_INCLUDE_DIRS(REMRTBIN_INCLUDE_DIRS)

  include_directories(${CMAKE_CURRENT_SOURCE_DIR})
  include_directories(SYSTEM ${TCL_INCLUDE_PATH})

  BRLCAD_ADDEXEC(remrt "../rt/opt.c;ihost.c;remrt.c" "liboptical;libdm")
  ADD_TARGET_DEPS(remrt dm_plugins)

  BRLCAD_ADDEXEC(rtsrv "../rt/usage.cpp;../rt/view.c;../rt/do.c;../rt/grid.c;../rt/heatgraph.c;../rt/opt.c;../rt/scanline.c;../rt/worker.c;rtsrv.c" "libdm;liboptical;libpkg;libicv")
  set_property(TARGET rtsrv APPEND PROPERTY COMPILE_DEFINITIONS "RTSRV")
  ADD_TARGET_DEPS(rtsrv dm_plugins)
endif (NOT WIN32)

CMAKEFILES(
  ihost.c
  ihost.h
  protocol.h
  remrt.c
  rtsrv.c
  )
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
