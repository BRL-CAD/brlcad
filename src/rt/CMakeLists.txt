set(RTBIN_INCLUDE_DIRS
  ${OPTICAL_INCLUDE_DIRS}
  ${FB_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

if(BRLCAD_ENABLE_OPENCL)
  add_definitions(-DUSE_OPENCL)
  set(RT_INCLUDE_DIRS ${RT_INCLUDE_DIRS} ${OPENCL_INCLUDE_DIRS})
endif (BRLCAD_ENABLE_OPENCL)

LIST(REMOVE_DUPLICATES RTBIN_INCLUDE_DIRS)
include_directories(${RTBIN_INCLUDE_DIRS})

set(LIBRTUIF_SOURCES
  do.c
  heatgraph.c
  main.c
  opt.c
  scanline.c
  worker.c
  )

BRLCAD_ADDLIB(librtuif "${LIBRTUIF_SOURCES}" "${M_LIBRARY}" STATIC NO_INSTALL)

add_library(librtuif_multispectral STATIC ${LIBRTUIF_SOURCES})
set_property(TARGET librtuif_multispectral APPEND PROPERTY LINKER_LANGUAGE C)
set_property(TARGET librtuif_multispectral APPEND PROPERTY COMPILE_DEFINITIONS "RT_MULTISPECTRAL")
set_property(TARGET librtuif_multispectral APPEND PROPERTY FOLDER "BRL-CAD Static Libraries")

if(BRLCAD_ENABLE_OPENCL)
  set(OPENCL_LIBS ${OPENCL_LIBRARIES})
endif(BRLCAD_ENABLE_OPENCL)

BRLCAD_ADDDATA(rtexample.c sample_applications)

BRLCAD_ADDEXEC(rtexample "rtexample.c" "librt;libfb;liboptical;libicv" NO_INSTALL)

BRLCAD_ADDEXEC(reshoot "reshoot.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv")

BRLCAD_ADDEXEC(rtwalk "rtwalk.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rtshot "rtshot.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rtregis "read-rtlog.c;rtregis.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv")

BRLCAD_ADDEXEC(rtscale "read-rtlog.c;rtscale.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rt "view.c" "${OPENCL_LIBS};${CMAKE_THREAD_LIBS_INIT};librtuif;librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rt_bot_faces "view_bot_faces.c" "${CMAKE_THREAD_LIBS_INIT};librtuif;librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rt_bot_faces APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtarea "viewarea.c" "${CMAKE_THREAD_LIBS_INIT};librtuif;librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtarea APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtcell "viewcell.c" "${CMAKE_THREAD_LIBS_INIT};librtuif;librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtcell APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtcheck "viewcheck.c" "${CMAKE_THREAD_LIBS_INIT};librtuif;librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtcheck APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtedge "viewedge.c" "${CMAKE_THREAD_LIBS_INIT};librtuif;librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rtfrac "viewfrac.c" "${CMAKE_THREAD_LIBS_INIT};librtuif;librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtfrac APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtg3 "viewg3.c" "${CMAKE_THREAD_LIBS_INIT};librtuif;librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtg3 APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rthide "viewhide.c" "${CMAKE_THREAD_LIBS_INIT};librtuif;librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rthide APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtpp "viewpp.c" "${CMAKE_THREAD_LIBS_INIT};librtuif;librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rtrad "viewrad.c" "${CMAKE_THREAD_LIBS_INIT};librtuif;librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtrad APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtrange "viewrange.c" "${CMAKE_THREAD_LIBS_INIT};librtuif;librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtrange APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtray "viewray.c" "${CMAKE_THREAD_LIBS_INIT};librtuif;librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtray APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtsil "viewsil.c" "${CMAKE_THREAD_LIBS_INIT};librtuif;librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rtweight "viewweight.c" "${CMAKE_THREAD_LIBS_INIT};librtuif;librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtweight APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtxray "viewxray.c" "${CMAKE_THREAD_LIBS_INIT};librtuif;librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")

set(rt_ignore_files
  ext.h
  libfb-dummy.c
  main.c
  rad.h
  rtuif.h
  scanline.h
  viewdir.c
  viewdummy.c
  viewscat.c
  )
CMAKEFILES(${rt_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
