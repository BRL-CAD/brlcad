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

  if(TARGET mged AND TARGET dm-tkswrast AND TARGET dm-X AND TARGET dm-ogl
     AND TARGET icv AND TARGET pixcrop AND TARGET rt)
    brlcad_add_xmin_test(
      NAME regress-mged-xmin-gui-edit
      TIMEOUT 600
      ENVIRONMENT
        "CMAKE_COMMAND=${CMAKE_COMMAND}"
        "ICV_BIN=$<TARGET_FILE:icv>"
        "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR}"
        "MGED_BIN=$<TARGET_FILE:mged>"
        "MGED_DM_TYPES=tkswrast X ogl"
        "MGED_GUI_APNG_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        "MGED_GUI_MANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/mged/mged_gui_menu_manifest.txt"
        "MGED_PIPE_ORACLE=${CMAKE_CURRENT_SOURCE_DIR}/mged/xmin_gui_pipe_oracle.tcl"
        "MGED_RT_BIN=$<TARGET_FILE:rt>"
        "MGED_XMIN_ACTIVE_SESSION=1"
        "MGED_TEST_SHELL=${SH_EXEC}"
        "MGED_XMIN_RC=${CMAKE_CURRENT_SOURCE_DIR}/mged/xmin_gui_edit.tcl"
        "PIXCROP_BIN=$<TARGET_FILE:pixcrop>"
        "XMIN_GUI_LIBRARY=${CMAKE_SOURCE_DIR}/regress/xmin/xmin_gui_test.tcl"
      COMMAND "${SH_EXEC}" "${CMAKE_CURRENT_SOURCE_DIR}/mged/mged_xmin_gui.sh"
    )
    brlcad_regression_test(regress-mged-xmin-gui-edit
      "mged;dm-tkswrast;dm-X;dm-ogl;icv;pixcrop;rt"
      STAND_ALONE TEST_DEFINED)
  endif()

  if(TARGET mged AND TARGET dm-tkswrast)
    brlcad_add_xmin_test(
      NAME regress-mged-xmin-gui-controls
      TIMEOUT 180
      ENVIRONMENT ${_xmin_smoke_environment}
        "XMIN_GUI_LIBRARY=${_xmin_gui_library}"
      COMMAND "${SH_EXEC}" "${_xmin_tcl_runner}"
        mged-controls $<TARGET_FILE:mged> --gui --dm-type tkswrast
        --rcfile "${CMAKE_CURRENT_SOURCE_DIR}/mged/xmin_gui_controls.tcl"
        --geom 680x520+0+0 --ggeom 512x512+700+0
        "${_xmin_test_db}"
    )
    brlcad_regression_test(regress-mged-xmin-gui-controls
      "mged;dm-tkswrast;m35.g" STAND_ALONE TEST_DEFINED)

    brlcad_add_xmin_test(
      NAME regress-mged-xmin-gui-workflows
      TIMEOUT 300
      ENVIRONMENT ${_xmin_smoke_environment}
        "XMIN_GUI_LIBRARY=${_xmin_gui_library}"
      COMMAND "${SH_EXEC}" "${_xmin_tcl_runner}"
        mged-workflows $<TARGET_FILE:mged> --gui --dm-type tkswrast
        --rcfile "${CMAKE_CURRENT_SOURCE_DIR}/mged/xmin_gui_workflows.tcl"
        --geom 680x520+0+0 --ggeom 512x512+700+0
        "${_xmin_test_db}"
    )
    brlcad_regression_test(regress-mged-xmin-gui-workflows
      "mged;dm-tkswrast;m35.g" STAND_ALONE TEST_DEFINED)

    function(mged_add_xmin_gui_fixture test_name dependencies)
      string(REPLACE "-" "_" script_name "${test_name}")
      brlcad_add_xmin_test(
        NAME regress-mged-xmin-gui-${test_name}
        TIMEOUT 180
        ENVIRONMENT ${_xmin_smoke_environment}
          "MGED_XMIN_GUI_LIBRARY=${CMAKE_CURRENT_SOURCE_DIR}/mged/xmin_gui_test.tcl"
          "XMIN_GUI_LIBRARY=${_xmin_gui_library}"
          ${ARGN}
        COMMAND "${SH_EXEC}" "${_xmin_tcl_runner}"
          mged-${test_name} $<TARGET_FILE:mged> --gui --dm-type tkswrast
          --rcfile "${CMAKE_CURRENT_SOURCE_DIR}/mged/xmin_gui_${script_name}.tcl"
          --geom 680x520+0+0 --ggeom 512x512+700+0
          "${_xmin_test_db}"
      )
      brlcad_regression_test(regress-mged-xmin-gui-${test_name}
        "${dependencies}" STAND_ALONE TEST_DEFINED)
    endfunction()

    mged_add_xmin_gui_fixture(lod
      "mged;dm-tkswrast;m35.g;plot3-asc"
      "PLOT3_ASC_BIN=$<TARGET_FILE:plot3-asc>"
    )
    mged_add_xmin_gui_fixture(render-outputs
      "mged;dm-tkswrast;m35.g;plot3-asc;rt"
      "PLOT3_ASC_BIN=$<TARGET_FILE:plot3-asc>"
    )
    mged_add_xmin_gui_fixture(help "mged;dm-tkswrast;m35.g")
    mged_add_xmin_gui_fixture(bot "mged;dm-tkswrast;m35.g")
    mged_add_xmin_gui_fixture(editors "mged;dm-tkswrast;m35.g")
    mged_add_xmin_gui_fixture(query-pattern "mged;dm-tkswrast;m35.g")
    mged_add_xmin_gui_fixture(overlaps
      "mged;dm-tkswrast;m35.g;gchecker"
    )
    mged_add_xmin_gui_fixture(shotvis "mged;dm-tkswrast;m35.g")
  endif()

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
  mged/MGED_GUI_TEST_COVERAGE.md
  mged/mged_gui_menu_manifest.txt
  mged/mged_xmin_gui.sh
  mged/xmin_gui_bot.tcl
  mged/xmin_gui_controls.tcl
  mged/xmin_gui_editors.tcl
  mged/xmin_gui_help.tcl
  mged/xmin_gui_lod.tcl
  mged/xmin_gui_overlaps.tcl
  mged/xmin_gui_query_pattern.tcl
  mged/xmin_gui_render_outputs.tcl
  mged/xmin_gui_shotvis.tcl
  mged/xmin_gui_test.tcl
  mged/xmin_gui_workflows.tcl
  mged/xmin_gui_edit.tcl
  mged/xmin_gui_pipe_oracle.tcl
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
