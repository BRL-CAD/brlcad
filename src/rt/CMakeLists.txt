if(BRLCAD_ENABLE_OPENCL)
  add_definitions(-DUSE_OPENCL)
endif (BRLCAD_ENABLE_OPENCL)

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

BRLCAD_ADDLIB(librtuif "${LIBRTUIF_SOURCES}" "librt;${M_LIBRARY}" "" "${CMAKE_CURRENT_SOURCE_DIR}" STATIC NO_INSTALL)

if(BRLCAD_ENABLE_OPENCL)
  set(OPENCL_LIBS ${OPENCL_LIBRARIES})
endif(BRLCAD_ENABLE_OPENCL)

set(RT_STD_LIBS
  Threads::Threads
  )
if(MPI_C_LIBRARIES AND BRLCAD_ENABLE_MPI)
  set(RT_STD_LIBS ${RT_STD_LIBS}
    ${MPI_C_LIBRARIES}
    )
endif(MPI_C_LIBRARIES AND BRLCAD_ENABLE_MPI)
set(RT_STD_LIBS ${RT_STD_LIBS}
  librt
  libnmg
  libdm
  liboptical
  libicv
  ${M_LIBRARY}
  )

BRLCAD_ADDDATA(rtexample.c sample_applications)

BRLCAD_ADDEXEC(rtexample "rtexample.c" "librt;libdm;liboptical;libicv" NO_INSTALL)

BRLCAD_ADDEXEC(reshoot "reshoot.c" "Threads::Threads;librt;libdm;liboptical;libicv" FOLDER Rt)
target_include_directories(reshoot BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

BRLCAD_ADDEXEC(rtshot "rtshot.c" "${RT_STD_LIBS}" FOLDER Rt)
target_include_directories(rtshot BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
ADD_TARGET_DEPS(rtshot dm_plugins)

BRLCAD_ADDEXEC(rtscale "read-rtlog.c;rtscale.c" "Threads::Threads;librt;libdm;liboptical;libicv;${M_LIBRARY}" FOLDER Rt)
target_include_directories(rtscale BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
ADD_TARGET_DEPS(rtscale dm_plugins)

BRLCAD_ADDEXEC(rt "${LIBRTUIF_SOURCES};view.c" "${OPENCL_LIBS};${RT_STD_LIBS}" FOLDER Rt)
target_include_directories(rt BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
ADD_TARGET_DEPS(rt dm_plugins dm-txt)

BRLCAD_ADDEXEC(rtarea "${LIBRTUIF_SOURCES};viewarea.c" "${RT_STD_LIBS}" FOLDER Rt)
target_include_directories(rtarea BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET rtarea APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtcheck "${LIBRTUIF_SOURCES};viewcheck.c" "${RT_STD_LIBS}" FOLDER Rt)
target_include_directories(rtcheck BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET rtcheck APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtedge "${LIBRTUIF_SOURCES};viewedge.c" "${RT_STD_LIBS}" FOLDER Rt)
target_include_directories(rtedge BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
ADD_TARGET_DEPS(rtedge dm_plugins)

BRLCAD_ADDEXEC(rtg3 "${LIBRTUIF_SOURCES};viewg3.c" "${RT_STD_LIBS}" FOLDER Rt)
target_include_directories(rtg3 BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET rtg3 APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rthide "${LIBRTUIF_SOURCES};viewhide.c" "${RT_STD_LIBS}" FOLDER Rt)
target_include_directories(rthide BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET rthide APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtsil "${LIBRTUIF_SOURCES};viewsil.c" "${RT_STD_LIBS}" FOLDER Rt)
target_include_directories(rtsil BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
ADD_TARGET_DEPS(rtxray dm_plugins)

BRLCAD_ADDEXEC(rtsurf "rtsurf.c;rtsurf_hits.cpp" "libanalyze;librt;libdm;liboptical;libicv")
target_include_directories(rtsurf BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

BRLCAD_ADDEXEC(rtweight "${LIBRTUIF_SOURCES};viewweight.c" "libanalyze;${RT_STD_LIBS}" FOLDER Rt)
target_include_directories(rtweight BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET rtweight APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

BRLCAD_ADDEXEC(rtxray "${LIBRTUIF_SOURCES};viewxray.c" "${RT_STD_LIBS}" FOLDER Rt)
target_include_directories(rtxray BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
ADD_TARGET_DEPS(rtxray dm_plugins)

BRLCAD_ADDEXEC(rtdepth "${LIBRTUIF_SOURCES};viewdepth.c" "${RT_STD_LIBS}" FOLDER Rt)
target_include_directories(rtdepth BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
ADD_TARGET_DEPS(rtdepth dm_plugins)

set(rt_ignore_files
  CMakeLists.txt
  ext.h
  libfb-dummy.c
  main.c
  rtuif.h
  rtsurf_hits.h
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
