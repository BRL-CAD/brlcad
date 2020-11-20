if(SH_EXEC)

  if (TARGET asc2g)

    BRLCAD_ADD_TEST(NAME regress-asc COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/asc.sh" ${CMAKE_SOURCE_DIR})
    BRLCAD_REGRESSION_TEST(regress-asc "asc2g;g2asc;gdiff;dbupgrade" TEST_DEFINED)

  endif (TARGET asc2g)

  BRLCAD_ADD_TEST(NAME regress-asc2dsp COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/asc2dsp.sh" ${CMAKE_SOURCE_DIR})
  BRLCAD_REGRESSION_TEST(regress-asc2dsp "cv;asc2pix;pix-bw;asc2dsp" TEST_DEFINED)

endif(SH_EXEC)

CMAKEFILES(
  asc.sh
  v4.g
  asc2dsp.sh
  )

# list of temporary files
set(asc_outfiles
  asc.log
  asc2dsp.log
  asc2dsp-new.dsp
  asc2dsp-old.bw
  asc2dsp-old.dsp
  asc2dsp-old.pix
  moss_v4.asc
  moss_v4.g
  moss_v4_asc_v4.g
  moss_v4_asc_v4_v5.g
  moss_v4_asc_v5.g
  moss_v5.asc
  moss_v5.g
  moss_v5_basic.g
  v4.asc
  )

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${asc_outfiles}")
DISTCLEAN(${asc_outfiles})

CMAKEFILES(
  CMakeLists.txt
  asc2dsp.sh
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
