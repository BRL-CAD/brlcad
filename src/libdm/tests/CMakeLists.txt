
#  ************ libdm tests *************

BRLCAD_ADDEXEC(dm_test dm_test.c "libdm;libbu" TEST)

#TODO - these should be portable without X11, but we need to set up the Tk Xlib
# and provide an appropriate include first...
if (BRLCAD_ENABLE_TK AND BRLCAD_ENABLE_X11)
  include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${TCL_INCLUDE_PATH}
    ${TK_INCLUDE_PATH}
    ${X11_INCLUDE_DIR}
    )

  BRLCAD_ADDEXEC(tcl_img tcl_img.cpp "libdm;libbu;${TCL_LIBRARY};${TK_LIBRARY}" TEST_USEDATA)
  BRLCAD_ADDEXEC(tcl_tevent tcl_tevent.cpp "libdm;libbu;${TCL_LIBRARY};${TK_LIBRARY}" TEST_USEDATA)
endif ()

CMAKEFILES(CMakeLists.txt)
CMAKEFILES(
  tcl_img.cpp
  tcl_tevent.cpp
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
