# Tests for remrt (network distributed ray tracing)
#
# Two sub-tests are executed by a single C++ orchestrator binary, both
# using the libpkg ipc local-worker transport (socketpair):
#
#   1. IPC 512x512 benchmark render:
#      remrt auto-spawns one rtsrv worker via "host localhost always local"
#      in .remrtrc (add_host_local / BU_IPC_ADDR env var).  Output is
#      compared pixel-for-pixel against bench/ref/m35.pix to verify correct
#      distributed rendering.
#
#   2. IPC 64x64 quick render:
#      Same local-worker mechanism, 64x64 pixels, output size check only.
#
# An OpenSSL build also tests the benchmark over TCP with a pinned TLS
# connection to a foreground rtsrv worker.

if(TARGET remrt AND TARGET rtsrv)

  # Build the test orchestrator binary.
  brlcad_addexec(regress_remrt regress_remrt.cpp "libbu;Threads::Threads" TEST)
  if(TARGET regress_remrt)

    set_target_properties(regress_remrt PROPERTIES FOLDER "BRL-CAD Regression Tests/remrt")

    # Paths substituted at cmake configure time.
    set(M35G_PATH "${PROJECT_BINARY_DIR}/${DATA_DIR}/db/m35.g")
    set(REFPIX_PATH "${PROJECT_SOURCE_DIR}/bench/ref/m35.pix")

    brlcad_add_test(
      NAME regress-remrt
      COMMAND regress_remrt
        --remrt  $<TARGET_FILE:remrt>
        --m35g   "${M35G_PATH}"
        --refpix "${REFPIX_PATH}"
    )
    # Set the working directory to this CMake binary dir so that both the
    # test orchestrator and the remrt child process find ".remrtrc" in the
    # same known location.
    set_tests_properties(regress-remrt PROPERTIES
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    # TIMEOUT 600: a 512x512 distributed render plus startup overhead may
    # take several minutes.
    brlcad_regression_test(
      regress-remrt "remrt;rtsrv;m35.g;regress_remrt"
      TEST_DEFINED TIMEOUT 600
    )

    if(BUILD_TESTING AND (OpenSSL_FOUND OR OPENSSL_FOUND))
      set(REMRT_TLS_TEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/tls")
      file(MAKE_DIRECTORY "${REMRT_TLS_TEST_DIR}")
      brlcad_add_test(
        NAME regress-remrt-tls
        COMMAND regress_remrt
          --remrt  $<TARGET_FILE:remrt>
          --tls-worker $<TARGET_FILE:rtsrv>
          --m35g   "${M35G_PATH}"
          --refpix "${REFPIX_PATH}"
      )
      set_tests_properties(regress-remrt-tls PROPERTIES
        WORKING_DIRECTORY "${REMRT_TLS_TEST_DIR}"
      )
      brlcad_regression_test(
        regress-remrt-tls "remrt;rtsrv;m35.g;regress_remrt"
        TEST_DEFINED TIMEOUT 600
      )
      distclean("${REMRT_TLS_TEST_DIR}")
    endif()

    distclean(
      ${CMAKE_CURRENT_BINARY_DIR}/.remrtrc
      ${CMAKE_CURRENT_BINARY_DIR}/m35_remrt_ipc_bench.pix
      ${CMAKE_CURRENT_BINARY_DIR}/m35_remrt_ipc_bench.pix.0
      ${CMAKE_CURRENT_BINARY_DIR}/m35_remrt_ipc_quick.pix
      ${CMAKE_CURRENT_BINARY_DIR}/m35_remrt_ipc_quick.pix.0
    )

  endif(TARGET regress_remrt)

endif(TARGET remrt AND TARGET rtsrv)

cmakefiles(
  CMakeLists.txt
  regress_remrt.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
