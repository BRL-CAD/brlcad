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

set(RT_COMMON_SRCS
  do.c
  main.c
  opt.c
  worker.c
  )
if(NOT MSVC)
  set(RT_COMMON_SRCS ${RT_COMMON_SRCS} heatgraph.c)
endif(NOT MSVC)

if(BRLCAD_ENABLE_OPENCL)
  set(OPENCL_LIBS ${OPENCL_LIBRARIES})
endif(BRLCAD_ENABLE_OPENCL)

BRLCAD_ADDDATA(rtexample.c sample_applications)

BRLCAD_ADDEXEC(rtexample "rtexample.c" "librt;libfb;liboptical;libicv" NO_INSTALL)

BRLCAD_ADDEXEC(reshoot "reshoot.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv")

BRLCAD_ADDEXEC(rtwalk "rtwalk.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rtshot "rtshot.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rtregis "read-rtlog.c;rtregis.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv")

BRLCAD_ADDEXEC(rtscale "read-rtlog.c;rtscale.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rt "${RT_COMMON_SRCS};scanline.c;view.c" "${OPENCL_LIBS};${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rt_bot_faces "${RT_COMMON_SRCS};view_bot_faces.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rt_bot_faces APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtarea "${RT_COMMON_SRCS};viewarea.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtarea APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtcell "${RT_COMMON_SRCS};viewcell.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtcell APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtcheck "${RT_COMMON_SRCS};viewcheck.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtcheck APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtedge "${RT_COMMON_SRCS};viewedge.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rtfrac "${RT_COMMON_SRCS};viewfrac.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtfrac APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtg3 "${RT_COMMON_SRCS};viewg3.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtg3 APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rthide "${RT_COMMON_SRCS};viewhide.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rthide APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtpp "${RT_COMMON_SRCS};viewpp.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rtrad "${RT_COMMON_SRCS};viewrad.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtrad APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtrange "${RT_COMMON_SRCS};viewrange.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtrange APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtray "${RT_COMMON_SRCS};viewray.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtray APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtsil "${RT_COMMON_SRCS};viewsil.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")

BRLCAD_ADDEXEC(rtweight "${RT_COMMON_SRCS};viewweight.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")
set_property(TARGET rtweight APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtxray "${RT_COMMON_SRCS};viewxray.c" "${CMAKE_THREAD_LIBS_INIT};librt;libfb;liboptical;libicv;${M_LIBRARY}")

set(LIBREMRT_SOURCES
  do.c
  opt.c
  scanline.c
  sh_tcl.c
  view.c
  worker.c
  )
if(NOT MSVC)
  set(LIBREMRT_SOURCES ${LIBREMRT_SOURCES} heatgraph.c)
endif(NOT MSVC)

add_library(libremrt STATIC ${LIBREMRT_SOURCES})
if(M_LIBRARY)
  target_link_libraries(libremrt ${M_LIBRARY})
endif(M_LIBRARY)
set_target_properties(libremrt PROPERTIES FOLDER "BRL-CAD Static Libraries")

add_library(librttherm STATIC ${RT_COMMON_SRCS})
set_target_properties(librttherm PROPERTIES LINKER_LANGUAGE C)
set_target_properties(librttherm PROPERTIES FOLDER "BRL-CAD Static Libraries")

set(rt_ignore_files
  ext.h
  libfb-dummy.c
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
