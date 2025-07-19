set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/regress-ttcp.log")
set(TTCP_CLEAN_FILES
  ${CMAKE_CURRENT_BINARY_DIR}/ttcp_test_in.bin
  ${CMAKE_CURRENT_BINARY_DIR}/ttcp_test_out.bin
  ${CMAKE_CURRENT_BINARY_DIR}/ttcp_receiver.sh
  ${CMAKE_CURRENT_BINARY_DIR}/ttcp_receiver.bat
  ${CMAKE_CURRENT_BINARY_DIR}/ttcp_receiver.err
  ${CMAKE_CURRENT_BINARY_DIR}/ttcp_transmitter.err
  )

brlcad_regression_test(regress-ttcp "ttcp" EXEC ttcp)

distclean(${LOG_FILE})
distclean(${TTCP_CLEAN_FILES})

cmakefiles(
  CMakeLists.txt
  regress-ttcp.cmake.in
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
