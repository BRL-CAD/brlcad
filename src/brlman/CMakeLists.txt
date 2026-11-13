brlcad_find_package(gumbo CONFIG REQUIRED)
include(CheckCXXSymbolExists)
check_cxx_symbol_exists(wcwidth "wchar.h" BRLMAN_HAVE_WCWIDTH)

set(brlman_libs libbu ${IMM32_LIBRARY} ${COMCTL32_LIBRARY})

# Tcl/Tk version of brlman GUI
if(NOT BRLCAD_ENABLE_QT AND TARGET libtclcad AND TK_LIBRARY)
  brlcad_adddata(brlman.tcl tclscripts/brlman)

  set(brlman_libs libtclcad ${TCL_LIBRARY} ${TK_LIBRARY} ${brlman_libs})
endif(NOT BRLCAD_ENABLE_QT AND TARGET libtclcad AND TK_LIBRARY)

# Qt version of brlman GUI
if(BRLCAD_ENABLE_QT)
  find_package_qt(REQUIRED)

  if(Qt6Widgets_FOUND)
    set(brlman_libs libqtcad Qt6::Widgets Qt6::Gui ${brlman_libs})
  else()
    set(brlman_libs libqtcad Qt5::Widgets Qt5::Gui ${brlman_libs})
  endif(Qt6Widgets_FOUND)
endif(BRLCAD_ENABLE_QT)

# Console subsystem is required for shell invocation and redirected output.
brlcad_addexec(brlman "brlman.cpp;html_text.cpp;console.cpp" "${brlman_libs};gumbo" ARG_SCHEMA)
target_include_directories(brlman PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../libtermio)
if(BRLMAN_HAVE_WCWIDTH)
  target_compile_definitions(brlman PRIVATE BRLMAN_HAVE_WCWIDTH)
endif()

brlcad_addexec(brlman_text_test "html_text_test.cpp;html_text.cpp" "gumbo;libbu" TEST)
if(TARGET brlman_text_test)
  if(BRLMAN_HAVE_WCWIDTH)
    target_compile_definitions(brlman_text_test PRIVATE BRLMAN_HAVE_WCWIDTH)
  endif()
  brlcad_add_test(NAME brlman-html-text COMMAND brlman_text_test)
endif()
if(BRLCAD_EXTRADOCS)
  brlcad_add_test(NAME brlman-console-output
    COMMAND ${CMAKE_COMMAND} -DBRLMAN=$<TARGET_FILE:brlman>
      -P ${CMAKE_CURRENT_SOURCE_DIR}/console_test.cmake)
  if(TEST brlman-console-output)
    # The console test reads the generated manual page at runtime.
    add_custom_target(brlman-console-test-data
      DEPENDS "${PROJECT_BINARY_DIR}/${DOC_DIR}/html/man1/brlman.html")
    add_dependencies(unit brlman-console-test-data)
    add_dependencies(check brlman-console-test-data)
  endif()
endif()
if(NOT BRLCAD_ENABLE_QT AND TARGET libtclcad AND TK_LIBRARY)
  set(reflow_test_command ${TCL_TCLSH} "${CMAKE_CURRENT_SOURCE_DIR}/reflow_test.tcl"
    $<TARGET_FILE:libtclcad> $<TARGET_FILE:asciiquack>)
  if(BRLCAD_X11_PROVIDER_RESOLVED STREQUAL "XMIN")
    brlcad_add_gui_test(NAME brlman-html-reflow SCREEN 2560x1200x24
      COMMAND ${reflow_test_command})
  else()
    brlcad_add_test(NAME brlman-html-reflow COMMAND ${reflow_test_command})
  endif()
  if(TEST brlman-html-reflow)
    set_tests_properties(brlman-html-reflow PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)
  endif()
  # A usable desktop is required; run this explicitly during viewer work.
  add_custom_target(brlman-navigation-test
    COMMAND ${TCL_TCLSH} "${CMAKE_CURRENT_SOURCE_DIR}/navigation_test.tcl"
      $<TARGET_FILE:libtclcad>
    DEPENDS libtclcad
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    USES_TERMINAL
    VERBATIM)
  set_target_properties(brlman-navigation-test PROPERTIES FOLDER "BRL-CAD Validation Testing")
endif()
if(BRLCAD_ENABLE_QT)
  target_compile_definitions(brlman PRIVATE USE_QT)
endif(BRLCAD_ENABLE_QT)
if(TK_LIBRARY)
  target_compile_definitions(brlman PRIVATE HAVE_TK)
endif(TK_LIBRARY)

if(NOT BRLCAD_ENABLE_QT AND TARGET brlman_tcl_cp)
  add_dependencies(brlman brlman_tcl_cp)
  add_dependencies(brlman TclIndexBld)
endif(NOT BRLCAD_ENABLE_QT AND TARGET brlman_tcl_cp)

if(TARGET TclIndexBld)
  add_dependencies(brlman TclIndexBld)
endif(TARGET TclIndexBld)

cmakefiles(
  CMakeLists.txt
  brlman.cpp
  html_text.h
  html_text.cpp
  console.cpp
  html_text_test.cpp
  console_test.cmake
  reflow_test.tcl
  navigation_test.tcl
  brlman.tcl
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
