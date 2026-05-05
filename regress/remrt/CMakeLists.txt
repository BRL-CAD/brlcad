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
# No TCP ports are used; no manual rtsrv invocation is needed.
# The test therefore carries no RESOURCE_LOCK on any network port.

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
    # same known location.  bu_dir(BU_DIR_CURR) in the test code and
    # bu_dir(BU_DIR_CURR) in remrt's read_rc_file() both resolve to this dir.
    # RESOURCE_LOCK serialises this test with any other test that also binds
    # loopback TCP ports in the 4446+ range, so it can safely run as part of
    # the normal "check" / "Regression" target without a separate STAND_ALONE
    # label.
    set_tests_properties(regress-remrt PROPERTIES
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    # No STAND_ALONE: this test carries the standard "Regression" label so
    # it participates in the normal "ninja check" run.  Port isolation is
    # handled by the RESOURCE_LOCK above.
    # TIMEOUT 600: a 512x512 distributed render plus startup overhead may
    # take several minutes.
    brlcad_regression_test(
      regress-remrt "remrt;rtsrv;m35.g;regress_remrt"
      TEST_DEFINED TIMEOUT 600
    )

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
