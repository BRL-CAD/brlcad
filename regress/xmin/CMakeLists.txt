if(BRLCAD_X11_PROVIDER_RESOLVED STREQUAL "XMIN" AND
   Xmin_RUN_EXECUTABLE AND SH_EXEC AND TARGET m35.g)
  set(_xmin_smoke "${CMAKE_CURRENT_SOURCE_DIR}/xmin_gui_smoke.sh")
  set(_xmin_smoke_environment
    "CMAKE_COMMAND=${CMAKE_COMMAND}"
    "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR}"
    "XMIN_TEST_SHELL=${CMAKE_CURRENT_SOURCE_DIR}/xmin_test.sh"
  )
  set(_xmin_test_db "${CMAKE_BINARY_DIR}/${DATA_DIR}/db/m35.g")
  set(_xmin_gui_library "${CMAKE_CURRENT_SOURCE_DIR}/xmin_gui_test.tcl")
  set(_xmin_tcl_runner "${CMAKE_CURRENT_SOURCE_DIR}/xmin_tcl_gui_test.sh")

  if(TARGET archer)
    brlcad_add_xmin_test(
      NAME regress-archer-xmin-smoke
      TIMEOUT 180
      ENVIRONMENT ${_xmin_smoke_environment}
        "XMIN_SMOKE_ACTION=${CMAKE_CURRENT_SOURCE_DIR}/archer_action.sh"
        "XMIN_MIN_CHANGED_BYTES=10000"
      COMMAND "${SH_EXEC}" "${_xmin_smoke}" "Archer ${BRLCAD_VERSION}"
        "${CMAKE_CURRENT_SOURCE_DIR}/archer_smoke.sh"
        $<TARGET_FILE:archer> "${_xmin_test_db}"
    )
    brlcad_regression_test(regress-archer-xmin-smoke
      "archer;m35.g" STAND_ALONE TEST_DEFINED)

    if(TARGET bwish)
      brlcad_add_xmin_test(
        NAME regress-archer-xmin-gui
        TIMEOUT 180
        ENVIRONMENT ${_xmin_smoke_environment}
          "XMIN_GUI_LIBRARY=${_xmin_gui_library}"
          "ARCHER_LAUNCH=${CMAKE_BINARY_DIR}/${DATA_DIR}/tclscripts/archer/init/archer_launch.tcl"
          "ARCHER_TEST_DATABASE=${_xmin_test_db}"
          "ARCHER_MENU_MANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/archer_menu_manifest.txt"
        COMMAND "${SH_EXEC}" "${_xmin_tcl_runner}" archer
          $<TARGET_FILE:bwish> "${CMAKE_CURRENT_SOURCE_DIR}/archer_xmin_gui.tcl"
      )
      brlcad_regression_test(regress-archer-xmin-gui
        "archer;bwish;m35.g" STAND_ALONE TEST_DEFINED)

      brlcad_add_xmin_test(
        NAME regress-archer-embedded-xmin-gui
        TIMEOUT 180
        ENVIRONMENT ${_xmin_smoke_environment}
          "XMIN_GUI_LIBRARY=${_xmin_gui_library}"
        COMMAND "${SH_EXEC}" "${_xmin_tcl_runner}" archer-embedded
          $<TARGET_FILE:bwish>
          "${CMAKE_CURRENT_SOURCE_DIR}/archer_embedded_xmin_gui.tcl"
      )
      brlcad_regression_test(regress-archer-embedded-xmin-gui
        "archer;bwish" STAND_ALONE TEST_DEFINED)
    endif()
  endif()

  if(TARGET rtwizard)
    brlcad_add_xmin_test(
      NAME regress-rtwizard-xmin-smoke
      TIMEOUT 180
      ENVIRONMENT ${_xmin_smoke_environment}
        "XMIN_SMOKE_ACTION=${CMAKE_CURRENT_SOURCE_DIR}/rtwizard_action.sh"
        "XMIN_MIN_CHANGED_BYTES=5000"
      COMMAND "${SH_EXEC}" "${_xmin_smoke}" RtWizard
        $<TARGET_FILE:rtwizard> --gui -i "${_xmin_test_db}" -t A
    )
    brlcad_regression_test(regress-rtwizard-xmin-smoke
      "rtwizard;m35.g" STAND_ALONE TEST_DEFINED)

    if(TARGET bwish)
      brlcad_add_xmin_test(
        NAME regress-rtwizard-xmin-gui
        TIMEOUT 180
        ENVIRONMENT ${_xmin_smoke_environment}
          "XMIN_GUI_LIBRARY=${_xmin_gui_library}"
          "RTWIZARD_TEST_DATABASE=${_xmin_test_db}"
          "RTWIZARD_MENU_MANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/rtwizard_menu_manifest.txt"
        COMMAND "${SH_EXEC}" "${_xmin_tcl_runner}" rtwizard
          $<TARGET_FILE:bwish> "${CMAKE_CURRENT_SOURCE_DIR}/rtwizard_xmin_gui.tcl"
      )
      brlcad_regression_test(regress-rtwizard-xmin-gui
        "rtwizard;bwish;m35.g" STAND_ALONE TEST_DEFINED)
    endif()
  endif()

  # qged is still evolving, so keep this fixture deliberately narrow: prove
  # that Qt starts, the console accepts a command, and OpenGL redraws.
  if(TARGET qged)
    brlcad_add_xmin_test(
      NAME regress-qged-xmin-smoke
      TIMEOUT 180
      ENVIRONMENT ${_xmin_smoke_environment}
        "XMIN_SMOKE_ACTION=${CMAKE_CURRENT_SOURCE_DIR}/qged_action.sh"
        "XMIN_MIN_CHANGED_BYTES=5000"
      COMMAND "${SH_EXEC}" "${_xmin_smoke}" QGED
        "${CMAKE_CURRENT_SOURCE_DIR}/qged_smoke.sh"
        $<TARGET_FILE:qged> "${_xmin_test_db}"
    )
    brlcad_regression_test(regress-qged-xmin-smoke
      "qged;m35.g" STAND_ALONE TEST_DEFINED)
  endif()
endif()

cmakefiles(
  CMakeLists.txt
  README.md
  xmin_gui_smoke.sh
  archer_action.sh
  archer_smoke.sh
  archer_embedded_xmin_gui.tcl
  archer_menu_manifest.txt
  archer_xmin_gui.tcl
  qged_action.sh
  qged_smoke.sh
  rtwizard_action.sh
  rtwizard_menu_manifest.txt
  rtwizard_xmin_gui.tcl
  xmin_gui_test.tcl
  xmin_test.sh
  xmin_test.tcl
  xmin_tcl_gui_test.sh
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
