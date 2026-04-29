if(BRLCAD_ENABLE_OPENCL)
  add_definitions(-DUSE_OPENCL)
endif(BRLCAD_ENABLE_OPENCL)

set(
  LIBRTUIF_SOURCES
  do.c
  grid.c
  heatgraph.c
  main.c
  opt.c
  scanline.c
  usage.cpp
  worker.c
)

brlcad_addlib(librtuif "${LIBRTUIF_SOURCES}" "librt;${M_LIBRARY}" "" "${CMAKE_CURRENT_SOURCE_DIR}" STATIC NO_INSTALL)

if(BRLCAD_ENABLE_OPENCL)
  set(OPENCL_LIBS ${OPENCL_LIBRARIES})
endif(BRLCAD_ENABLE_OPENCL)

set(RT_STD_LIBS Threads::Threads)
if(MPI_C_LIBRARIES AND BRLCAD_ENABLE_MPI)
  set(RT_STD_LIBS ${RT_STD_LIBS} ${MPI_C_LIBRARIES})
endif(MPI_C_LIBRARIES AND BRLCAD_ENABLE_MPI)
set(
  RT_STD_LIBS
  ${RT_STD_LIBS}
  librt
  libnmg
  libdm
  liboptical
  libicv
  ${M_LIBRARY}
)

brlcad_adddata(rtexample.c sample_applications)

brlcad_addexec(rtexample "rtexample.c" "librt;libdm;liboptical;libicv" NO_INSTALL)

brlcad_addexec(reshoot "reshoot.c" "Threads::Threads;librt;libdm;liboptical;libicv" FOLDER Rt)
target_include_directories(reshoot BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

brlcad_addexec(rtshot "rtshot.c" "${RT_STD_LIBS}" FOLDER Rt)
target_include_directories(rtshot BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_target_deps(rtshot dm_plugins)

brlcad_addexec(rtscale "read-rtlog.c;rtscale.c" "Threads::Threads;librt;libdm;liboptical;libicv;${M_LIBRARY}" FOLDER Rt)
target_include_directories(rtscale BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_target_deps(rtscale dm_plugins)

brlcad_addexec(rt "${LIBRTUIF_SOURCES};view.c" "${OPENCL_LIBS};${RT_STD_LIBS}" FOLDER Rt)
target_include_directories(rt BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_target_deps(rt dm_plugins dm-txt)

brlcad_addexec(rtarea "${LIBRTUIF_SOURCES};viewarea.c" "${RT_STD_LIBS}" FOLDER Rt)
target_include_directories(rtarea BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET rtarea APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

brlcad_addexec(rtcheck "${LIBRTUIF_SOURCES};viewcheck.c" "${RT_STD_LIBS}" FOLDER Rt)
target_include_directories(rtcheck BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET rtcheck APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

brlcad_addexec(rtedge "${LIBRTUIF_SOURCES};viewedge.c" "${RT_STD_LIBS}" FOLDER Rt)
target_include_directories(rtedge BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_target_deps(rtedge dm_plugins)

brlcad_addexec(rtg3 "${LIBRTUIF_SOURCES};viewg3.c" "${RT_STD_LIBS}" FOLDER Rt)
target_include_directories(rtg3 BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET rtg3 APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

brlcad_addexec(rthide "${LIBRTUIF_SOURCES};viewhide.c" "${RT_STD_LIBS}" FOLDER Rt)
target_include_directories(rthide BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET rthide APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

brlcad_addexec(rtsil "${LIBRTUIF_SOURCES};viewsil.c" "${RT_STD_LIBS}" FOLDER Rt)
target_include_directories(rtsil BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_target_deps(rtxray dm_plugins)

brlcad_addexec(rtsurf "rtsurf.c;rtsurf_hits.cpp" "libanalyze;librt;libdm;liboptical;libicv")
target_include_directories(rtsurf BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

brlcad_addexec(rtweight "${LIBRTUIF_SOURCES};viewweight.c" "libanalyze;${RT_STD_LIBS}" FOLDER Rt)
target_include_directories(rtweight BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET rtweight APPEND PROPERTY COMPILE_DEFINITIONS "RT_TXT_OUTPUT")

brlcad_addexec(rtxray "${LIBRTUIF_SOURCES};viewxray.c" "${RT_STD_LIBS}" FOLDER Rt)
target_include_directories(rtxray BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_target_deps(rtxray dm_plugins)

brlcad_addexec(rtdepth "${LIBRTUIF_SOURCES};viewdepth.c" "${RT_STD_LIBS}" FOLDER Rt)
target_include_directories(rtdepth BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_target_deps(rtdepth dm_plugins)

set(
  rt_ignore_files
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
cmakefiles(${rt_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
