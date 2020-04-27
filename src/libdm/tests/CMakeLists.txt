if (BRLCAD_ENABLE_TK)
  BRLCAD_ADDEXEC(tcl_img tcl_img.cpp "libdm;${TCL_LIBRARY};${TK_LIBRARY}" TEST_USEDATA)
endif (BRLCAD_ENABLE_TK)

CMAKEFILES(CMakeLists.txt)
CMAKEFILES(tcl_img.cpp)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

