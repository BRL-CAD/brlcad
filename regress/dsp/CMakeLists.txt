
if (SH_EXEC AND TARGET mged)

  if (NOT WIN32)
    BRLCAD_ADD_TEST(NAME regress-dsp COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/dsp.sh" ${CMAKE_SOURCE_DIR})
    BRLCAD_REGRESSION_TEST(regress-dsp "mged;cv;asc2pix;rt;pix-bw" TEST_DEFINED)
  else (NOT WIN32)
    message(WARNING "regress-dsp failes on some Windows platforms (pix-bw appears to hang on github runners) - skipping")
  endif (NOT WIN32)

endif (SH_EXEC AND TARGET mged)

CMAKEFILES(
  dsp.sh
  asc2dsp-new.asc
  asc2dsp-old.asc
  dsp-1-1.asc
  dsp-1-2.asc
  dsp-2-1.asc
  dsp-2-2.asc
  dsp-2-3.asc
  dsp-2-4.asc
  dsp-2-5.asc
  dsp-3-1.asc
  dsp-3-2.asc
  dsp-3-3.asc
  dsp-3-4.asc
  dsp-3-5.asc
  dsp-3-6.asc
  dsp-3-7.asc
  dsp-3-8.asc
  dsp-common.sh
  run-dsp-case-set-0.sh
  run-dsp-case-set-1.sh
  run-dsp-case-set-2.sh
  run-dsp-case-set-3.sh
  )

# list of temporary files
set(dsp_outfiles
  dsp-2-1.bw
  dsp-2-1.dsp
  dsp-2-1.g
  dsp-2-1.log
  dsp-2-1.pix
  dsp-2-1.rt.pix
  dsp-2-2.bw
  dsp-2-2.dsp
  dsp-2-2.g
  dsp-2-2.log
  dsp-2-2.pix
  dsp-2-2.rt.pix
  dsp-2-3.bw
  dsp-2-3.dsp
  dsp-2-3.g
  dsp-2-3.log
  dsp-2-3.pix
  dsp-2-3.rt.pix
  dsp-2-4.bw
  dsp-2-4.dsp
  dsp-2-4.g
  dsp-2-4.log
  dsp-2-4.pix
  dsp-2-4.rt.pix
  dsp-2-5.bw
  dsp-2-5.dsp
  dsp-2-5.g
  dsp-2-5.log
  dsp-2-5.pix
  dsp-2-5.rt.pix
  dsp-3-1.bw
  dsp-3-1.dsp
  dsp-3-1.g
  dsp-3-1.log
  dsp-3-1.pix
  dsp-3-1.rt.pix
  dsp-3-2.bw
  dsp-3-2.dsp
  dsp-3-2.g
  dsp-3-2.log
  dsp-3-2.pix
  dsp-3-2.rt.pix
  dsp-3-3.bw
  dsp-3-3.dsp
  dsp-3-3.g
  dsp-3-3.log
  dsp-3-3.pix
  dsp-3-3.rt.pix
  dsp-3-4.bw
  dsp-3-4.dsp
  dsp-3-4.g
  dsp-3-4.log
  dsp-3-4.pix
  dsp-3-4.rt.pix
  dsp-3-5.bw
  dsp-3-5.dsp
  dsp-3-5.g
  dsp-3-5.log
  dsp-3-5.pix
  dsp-3-5.rt.pix
  dsp-3-6.bw
  dsp-3-6.dsp
  dsp-3-6.g
  dsp-3-6.log
  dsp-3-6.pix
  dsp-3-6.rt.pix
  dsp-3-7.bw
  dsp-3-7.dsp
  dsp-3-7.g
  dsp-3-7.log
  dsp-3-7.pix
  dsp-3-7.rt.pix
  dsp-3-8.bw
  dsp-3-8.dsp
  dsp-3-8.g
  dsp-3-8.log
  dsp-3-8.pix
  dsp-3-8.rt.pix
  dsp.log
  )

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${dsp_outfiles}")
DISTCLEAN(${dsp_outfiles})

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
