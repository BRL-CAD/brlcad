set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/regress-gdiff.log")
set(GDIFF_CLEAN_FILE ${CMAKE_CURRENT_BINARY_DIR}/gdiff.out)

brlcad_regression_test(regress-gdiff "gdiff" EXEC gdiff)

distclean(
  ${LOG_FILE}
  ${GDIFF_CLEAN_FILE}
  ${CMAKE_CURRENT_BINARY_DIR}/gdiff_ref.log
  ${CMAKE_CURRENT_BINARY_DIR}/gdiff_out.log
)

cmakefiles(
  CMakeLists.txt
  gdiff.ref
  regress-gdiff.cmake.in
  moss_1.g
  moss_2.g
  moss_3.g
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
