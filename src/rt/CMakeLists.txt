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
BRLCAD_INCLUDE_DIRS(RTBIN_INCLUDE_DIRS)

set(LIBRTUIF_SOURCES
  do.c
  grid.c
  heatgraph.c
  main.c
  opt.c
  scanline.c
  usage.cpp
  worker.c
  )

BRLCAD_ADDLIB(librtuif "${LIBRTUIF_SOURCES}" "librt;${M_LIBRARY}" STATIC NO_INSTALL)

if(BRLCAD_ENABLE_OPENCL)
  set(OPENCL_LIBS ${OPENCL_LIBRARIES})
endif(BRLCAD_ENABLE_OPENCL)

if(MPI_C_LIBRARIES AND BRLCAD_ENABLE_MPI)
  set(RT_STD_LIBS
    Threads::Threads
    ${MPI_C_LIBRARIES}
    librt
    libnmg
    liboptical
    libicv
    ${M_LIBRARY}
    )
else()
  set(RT_STD_LIBS
    Threads::Threads
    librt
    libnmg
    liboptical
    libicv
    ${M_LIBRARY}
    )
endif(MPI_C_LIBRARIES AND BRLCAD_ENABLE_MPI)

BRLCAD_ADDDATA(rtexample.c sample_applications)

BRLCAD_ADDEXEC(rtexample "rtexample.c" "librt;libdm;liboptical;libicv" NO_INSTALL)

BRLCAD_ADDEXEC(reshoot "reshoot.c" "Threads::Threads;librt;libdm;liboptical;libicv" FOLDER Rt)

BRLCAD_ADDEXEC(rtshot "rtshot.c" "${RT_STD_LIBS};libdm" FOLDER Rt)
ADD_TARGET_DEPS(rtshot dm_plugins)

BRLCAD_ADDEXEC(rtscale "read-rtlog.c;rtscale.c" "Threads::Threads;librt;libdm;liboptical;libicv;${M_LIBRARY}" FOLDER Rt)
ADD_TARGET_DEPS(rtscale dm_plugins)

BRLCAD_ADDEXEC(rt "${LIBRTUIF_SOURCES};view.c" "${OPENCL_LIBS};${RT_STD_LIBS};libdm" FOLDER Rt)
ADD_TARGET_DEPS(rt dm_plugins dm-txt)

BRLCAD_ADDEXEC(rtarea "${LIBRTUIF_SOURCES};viewarea.c" "${RT_STD_LIBS}" FOLDER Rt)
set_property(TARGET rtarea APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtcheck "${LIBRTUIF_SOURCES};viewcheck.c" "${RT_STD_LIBS}" FOLDER Rt)
set_property(TARGET rtcheck APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtedge "${LIBRTUIF_SOURCES};viewedge.c" "${RT_STD_LIBS};libdm" FOLDER Rt)
ADD_TARGET_DEPS(rtedge dm_plugins)

BRLCAD_ADDEXEC(rtg3 "${LIBRTUIF_SOURCES};viewg3.c" "${RT_STD_LIBS}" FOLDER Rt)
set_property(TARGET rtg3 APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rthide "${LIBRTUIF_SOURCES};viewhide.c" "${RT_STD_LIBS}" FOLDER Rt)
set_property(TARGET rthide APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtsil "${LIBRTUIF_SOURCES};viewsil.c" "${RT_STD_LIBS};libdm" FOLDER Rt)
ADD_TARGET_DEPS(rtxray dm_plugins)

BRLCAD_ADDEXEC(rtweight "${LIBRTUIF_SOURCES};viewweight.c" "libanalyze;${RT_STD_LIBS}" FOLDER Rt)
set_property(TARGET rtweight APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtxray "${LIBRTUIF_SOURCES};viewxray.c" "${RT_STD_LIBS};libdm" FOLDER Rt)
ADD_TARGET_DEPS(rtxray dm_plugins)

set(rt_ignore_files
  CMakeLists.txt
  ext.h
  libfb-dummy.c
  main.c
  rtuif.h
  scanline.h
  viewdir.c
  viewdummy.c
  )
CMAKEFILES(${rt_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
