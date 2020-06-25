set(REMRTBIN_INCLUDE_DIRS
  ${REMRT_INCLUDE_DIRS}
  ${OPTICAL_INCLUDE_DIRS}
  ${FB_INCLUDE_DIRS}
  ${PKG_INCLUDE_DIRS}
  )
LIST(REMOVE_DUPLICATES REMRTBIN_INCLUDE_DIRS)
include_directories(${REMRTBIN_INCLUDE_DIRS})

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${TCL_INCLUDE_PATH}
  )

BRLCAD_ADDEXEC(remrt "../rt/opt.c;ihost.c;remrt.c" "liboptical;libdm")

BRLCAD_ADDEXEC(rtsrv "../rt/usage.cpp;../rt/view.c;../rt/do.c;../rt/heatgraph.c;../rt/opt.c;../rt/scanline.c;../rt/worker.c;rtsrv.c" "libdm;liboptical;libpkg;libicv")
set_property(TARGET rtsrv APPEND PROPERTY COMPILE_DEFINITIONS "RTSRV")

CMAKEFILES(ihost.h protocol.h)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
