set(
  test_deps
  dm-swrast
  ged-close
  ged-draw
  ged-dm
  ged-facetize
  ged-open
  ged-png2fb
  ged-select
  ged-tol
  ged-view
  ged-zap
)

set(basic_view_test_ctrls basic.apng)
brlcad_addexec(ged_test_draw "basic.cpp;util.cpp" "libged;libdm;PNG::PNG;ZLIB::ZLIB" TEST)
if(TARGET ged_test_draw)
  foreach(tdp ${test_deps})
    if (TARGET tdp)
      add_dependencies(ged_test_draw ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_draw)
brlcad_add_test(NAME ged_test_drawing_basic COMMAND ged_test_draw -c "${CMAKE_CURRENT_SOURCE_DIR}")

set(faceplate_test_ctrls faceplate.apng)
brlcad_addexec(ged_test_faceplate "faceplate.cpp;util.cpp" "libged;libdm;PNG::PNG;ZLIB::ZLIB" TEST)
if(TARGET ged_test_faceplate)
  foreach(tdp ${test_deps})
    if (TARGET tdp)
      add_dependencies(ged_test_faceplate ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_faceplate)
brlcad_add_test(NAME ged_test_drawing_faceplate COMMAND ged_test_faceplate -c "${CMAKE_CURRENT_SOURCE_DIR}")

set(lod_test_ctrls lod.apng)
brlcad_addexec(ged_test_lod "lod.cpp;util.cpp" "libged;libdm;PNG::PNG;ZLIB::ZLIB" TEST)
if(TARGET ged_test_lod)
  foreach(tdp ${test_deps})
    if (TARGET tdp)
      add_dependencies(ged_test_lod ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_lod)
brlcad_add_test(NAME ged_test_drawing_lod COMMAND ged_test_lod -c "${CMAKE_CURRENT_SOURCE_DIR}")
distclean(${CMAKE_CURRENT_BINARY_DIR}/moss_lod_tmp.g)

set(select_test_ctrls select.apng)
brlcad_addexec(ged_test_select_draw "select.cpp;util.cpp" "libged;libdm;PNG::PNG;ZLIB::ZLIB" TEST)
if(TARGET ged_test_select_draw)
  foreach(tdp ${test_deps})
    if (TARGET tdp)
      add_dependencies(ged_test_select_draw ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_select_draw)
brlcad_add_test(NAME ged_test_drawing_select COMMAND ged_test_select_draw -c "${CMAKE_CURRENT_SOURCE_DIR}")
distclean(${CMAKE_CURRENT_BINARY_DIR}/moss_select_tmp.g)

set(quad_test_ctrls quad_00.apng quad_01.apng quad_02.apng quad_03.apng)
brlcad_addexec(ged_test_quad "quad.cpp;util.cpp" "libged;libdm;PNG::PNG;ZLIB::ZLIB" TEST)
if(TARGET ged_test_quad)
  foreach(tdp ${test_deps})
    if (TARGET tdp)
      add_dependencies(ged_test_quad ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_quad)
brlcad_add_test(NAME ged_test_drawing_quad COMMAND ged_test_quad -c "${CMAKE_CURRENT_SOURCE_DIR}")
distclean(${CMAKE_CURRENT_BINARY_DIR}/moss_quad_tmp.g)

# The idea here is to test view stability across resize events.
# Note on APNG animation playback: These control images combine test frames of
# different sizes (e.g. 512x512, 600x600). Viewers that play these animations 
# will use a global canvas size (600x600) and smaller frames will not fill 
# the canvas, causing viewers to flash a transparent or white background for 
# the unfilled space. This is purely visual when viewed natively, and the 
# extraction tool recovers the original exact frame dimensions perfectly.
set(aet_test_ctrls aet_00.apng aet_01.apng aet_02.apng aet_03.apng)
brlcad_addexec(ged_test_aet "aet.cpp;util.cpp" "libged;libdm;PNG::PNG;ZLIB::ZLIB" TEST)
if(TARGET ged_test_aet)
  foreach(tdp ${test_deps})
    if (TARGET tdp)
      add_dependencies(ged_test_aet ${tdp})
    endif ()
  endforeach()
endif(TARGET ged_test_aet)

cmakefiles(
  README
  empty.png
  moss.g
  moss.png
  rook.g
  util.cpp
  ${basic_view_test_ctrls}
  ${faceplate_test_ctrls}
  ${lod_test_ctrls}
  ${select_test_ctrls}
  ${quad_test_ctrls}
  ${aet_test_ctrls}
)

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
