set(RTTHERM_INCLUDE_DIRS
  ${FB_INCLUDE_DIRS}
  ${OPTICAL_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${MULTISPECTRAL_INCLUDE_DIRS}
  ${OPTICAL_INCLUDE_DIRS}
  )
LIST(REMOVE_DUPLICATES RTTHERM_INCLUDE_DIRS)
include_directories(${RTTHERM_INCLUDE_DIRS})

if(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_TK)
  BRLCAD_ADDEXEC(ssampview "spectrum.c;ssampview.c" "libfb;libmultispectral;${TCL_TK_LIBRARY};${X11_LIBRARIES}")
endif(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_TK)

BRLCAD_ADDEXEC(rttherm "spectrum.c;viewtherm.c" "libfb;libged;librttherm;libmultispectral")
SET_TARGET_PROPERTIES(rttherm PROPERTIES COMPILE_DEFINITIONS "RT_MULTISPECTRAL")
install(PROGRAMS ssampview.tcl DESTINATION ${BIN_DIR})

BRLCAD_ADDEXEC(ssamp-bw ssamp-bw.c libmultispectral)

set(rttherm_MANS
  rttherm.1
  ssamp-bw.1
  )
ADD_MAN_PAGES(1 rttherm_MANS)

CMAKEFILES(pixtest.c ssampview.tcl std_day_1km.dat test.sh)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
