#  ************ libdm tests *************

brlcad_addexec(dm_test dm_test.c "libdm;libbu" TEST)

# dm_dlist_sensor register/fire/clear
brlcad_addexec(test_dm_dlist_sensor test_dlist_sensor.c "libdm;libbv;libbu" TEST)
brlcad_add_test(NAME dm_dlist_sensor COMMAND test_dm_dlist_sensor)

#TODO - these should be portable without X11, but we need to set up the Tk Xlib
# and provide an appropriate include first...
if(BRLCAD_ENABLE_TK AND BRLCAD_ENABLE_X11)
  set(TCL_ENABLE_TK ON)
  find_package_tcl()

  if(TCL_FOUND AND TK_FOUND)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${TCL_INCLUDE_PATH} ${TK_INCLUDE_PATH} ${X11_INCLUDE_DIR})

    if(BUILD_TESTING)
      brlcad_addexec(tcl_img tcl_img.cpp "libdm;libbu;${TCL_LIBRARY};${TK_LIBRARY}" TEST_USEDATA)
      brlcad_addexec(tcl_tevent tcl_tevent.cpp "libdm;libbu;${TCL_LIBRARY};${TK_LIBRARY}" TEST_USEDATA)
    endif(BUILD_TESTING)
  endif(TCL_FOUND AND TK_FOUND)
endif()

cmakefiles(CMakeLists.txt)
cmakefiles(
  dm_test.c
  test_dlist_sensor.c
  tcl_img.cpp
  tcl_tevent.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
