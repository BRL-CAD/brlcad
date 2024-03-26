if (NOT WIN32)
  BRLCAD_ADDEXEC(remrt "../rt/opt.c;ihost.c;remrt.c" "liboptical;libdm")
  target_include_directories(remrt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  ADD_TARGET_DEPS(remrt dm_plugins)

  BRLCAD_ADDEXEC(rtsrv "../rt/usage.cpp;../rt/view.c;../rt/do.c;../rt/grid.c;../rt/heatgraph.c;../rt/opt.c;../rt/scanline.c;../rt/worker.c;rtsrv.c" "libdm;liboptical;libpkg;libicv")
  target_include_directories(rtsrv PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
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
