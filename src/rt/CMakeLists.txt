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
  usage.cpp
  worker.c
  )

BRLCAD_ADDLIB(librtuif "${LIBRTUIF_SOURCES}" "${M_LIBRARY}" STATIC NO_INSTALL)

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

BRLCAD_ADDEXEC(rt "${LIBRTUIF_SOURCES};view.c" "${OPENCL_LIBS};${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rt_bot_faces "${LIBRTUIF_SOURCES};view_bot_faces.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rt_bot_faces APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtarea "${LIBRTUIF_SOURCES};viewarea.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtarea APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtcell "${LIBRTUIF_SOURCES};viewcell.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtcell APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtcheck "${LIBRTUIF_SOURCES};viewcheck.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtcheck APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtedge "${LIBRTUIF_SOURCES};viewedge.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rtfrac "${LIBRTUIF_SOURCES};viewfrac.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtfrac APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtg3 "${LIBRTUIF_SOURCES};viewg3.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtg3 APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rthide "${LIBRTUIF_SOURCES};viewhide.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rthide APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtpp "${LIBRTUIF_SOURCES};viewpp.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rtrad "${LIBRTUIF_SOURCES};viewrad.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtrad APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtrange "${LIBRTUIF_SOURCES};viewrange.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtrange APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtray "${LIBRTUIF_SOURCES};viewray.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtray APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtsil "${LIBRTUIF_SOURCES};viewsil.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rtweight "${LIBRTUIF_SOURCES};viewweight.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;libanalyze;${M_LIBRARY}")
set_property(TARGET rtweight APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtxray "${LIBRTUIF_SOURCES};viewxray.c" "${CMAKE_THREAD_LIBS_INIT};librt;libnmg;libfb;liboptical;libicv;${M_LIBRARY}")

set(rt_ignore_files
  CMakeLists.txt
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
