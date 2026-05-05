# Regression tests for libpkg + fbs_open_ipc + libtclcad IPC path.
#
# Tests the code added in the "Expand libpkg ipc usage" PR:
#
#   Phase 1: fbs_open_ipc() + fbs_ipc_child_addr_env()  (libdm/fbserv.c)
#   Phase 2: BU_IPC_ADDR env-var detection               (libdm/if_remote.c)
#   Phase 4: Tcl_CreateFileHandler callback mechanism    (libtclcad/fbserv.c)
#   Phase 5: Qt QSocketNotifier IPC delivery             (src/qged/fbserv.cpp)
#   Phase 7: fbserv -I <addr> flag                       (src/fbserv/fbserv.c)
#
# regress_ipc     — headless C tests (no display required)
# test_qt_ipc     — Qt QSocketNotifier tests (QT_QPA_PLATFORM=offscreen)
# test_qged_ert   — full qged+ert integration under Xvfb (requires display)

# -------------------------------------------------------------------
# regress_ipc: headless tests (Phases 1, 2, 4, 7)
# -------------------------------------------------------------------
if(TARGET libbu AND TARGET libdm AND TARGET libpkg AND TARGET libtclcad AND TARGET fbserv)

  set(regress_ipc_libs libbu libdm libpkg libtclcad ${TCL_LIBRARY})

  brlcad_addexec(regress_ipc regress_ipc.cpp "${regress_ipc_libs}" TEST)
  if(TARGET regress_ipc)
    set_target_properties(regress_ipc PROPERTIES FOLDER "BRL-CAD Regression Tests/ipc")

    # Tell the test where to find the fbserv binary
    brlcad_add_test(
      NAME    regress-ipc
      COMMAND regress_ipc --verbose
    )
    set_tests_properties(regress-ipc PROPERTIES
      ENVIRONMENT "FBSERV_BIN=$<TARGET_FILE:fbserv>;LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib"
      TIMEOUT 120
    )
    brlcad_regression_test(regress-ipc "regress_ipc;fbserv;libbu;libdm;libpkg;libtclcad" STAND_ALONE TEST_DEFINED)
    distclean(${CMAKE_CURRENT_BINARY_DIR}/regress-ipc.log)
    distclean(${CMAKE_CURRENT_BINARY_DIR}/regress_ipc_fbserv.log)

  endif(TARGET regress_ipc)

endif(TARGET libbu AND TARGET libdm AND TARGET libpkg AND TARGET libtclcad AND TARGET fbserv)

# -------------------------------------------------------------------
# test_qt_ipc: Qt QSocketNotifier delivery tests (Phase 5)
# Requires Qt5 or Qt6 Widgets.  Uses QT_QPA_PLATFORM=offscreen so
# no real display is needed.
#
# In the BRL-CAD build system, BRLCAD_ENABLE_QT is the canonical
# cache variable indicating Qt support is available.  Imported Qt
# targets (Qt5::Widgets etc.) are created inside the src/ subtree
# via find_package_qt(); they are visible in the regress/ subtree
# only after that same find_package call has been issued here, so
# we re-invoke find_package_qt() if BRLCAD_ENABLE_QT is ON.
# -------------------------------------------------------------------
if(BRLCAD_ENABLE_QT AND TARGET libbu AND TARGET libdm AND TARGET libpkg)
  find_package_qt()
  set(qt_ipc_libs libbu libdm libpkg)
  if(Qt6Widgets_FOUND)
    list(APPEND qt_ipc_libs Qt6::Widgets Qt6::Core)
    set(HAVE_QT_IPC_DEPS TRUE)
  elseif(Qt5Widgets_FOUND)
    list(APPEND qt_ipc_libs Qt5::Widgets Qt5::Core)
    set(HAVE_QT_IPC_DEPS TRUE)
  endif()
endif()

if(HAVE_QT_IPC_DEPS)
  brlcad_addexec(test_qt_ipc test_qt_ipc.cpp "${qt_ipc_libs}" TEST)
  if(TARGET test_qt_ipc)
    set_target_properties(test_qt_ipc PROPERTIES FOLDER "BRL-CAD Regression Tests/ipc")

    brlcad_add_test(
      NAME    regress-qt-ipc
      COMMAND test_qt_ipc --verbose
    )
    set_tests_properties(regress-qt-ipc PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib"
      TIMEOUT 60
    )
    brlcad_regression_test(regress-qt-ipc "test_qt_ipc;libbu;libdm;libpkg" STAND_ALONE TEST_DEFINED)
    distclean(${CMAKE_CURRENT_BINARY_DIR}/regress-qt-ipc.log)

  endif(TARGET test_qt_ipc)
endif(HAVE_QT_IPC_DEPS)

# -------------------------------------------------------------------
# test_qged_ert: full qged+ert integration under Xvfb (Phase 3+5)
# Requires qged, xvfb-run, and xdotool.  Skips gracefully if absent.
# -------------------------------------------------------------------
find_program(XVFB_RUN xvfb-run)
find_program(XDOTOOL  xdotool)

if(TARGET qged AND XVFB_RUN AND XDOTOOL)
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/test_qged_ert.sh
    ${CMAKE_CURRENT_BINARY_DIR}/test_qged_ert.sh
    COPYONLY
  )
  file(CHMOD ${CMAKE_CURRENT_BINARY_DIR}/test_qged_ert.sh
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
  )
  brlcad_add_test(
    NAME    regress-qged-ert
    COMMAND ${XVFB_RUN} -a -s "-screen 0 1280x960x24"
            ${CMAKE_CURRENT_BINARY_DIR}/test_qged_ert.sh
  )
  set_tests_properties(regress-qged-ert PROPERTIES
    ENVIRONMENT "QGED_BIN=$<TARGET_FILE:qged>;RT_BIN=$<TARGET_FILE:rt>;TEST_DB=${CMAKE_BINARY_DIR}/share/db/boolean-ops.g;LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib;VERBOSE=1"
    TIMEOUT 120
  )
  brlcad_regression_test(regress-qged-ert "qged;rt" STAND_ALONE TEST_DEFINED)
  distclean(${CMAKE_CURRENT_BINARY_DIR}/regress-qged-ert.log)
endif()

cmakefiles(
  CMakeLists.txt
  regress_ipc.cpp
  test_qt_ipc.cpp
  test_qged_ert.sh
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
