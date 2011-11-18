SET(IRPREP_INCLUDE_DIRS
	${BU_INCLUDE_DIRS}
	${RT_INCLUDE_DIRS}
	${X11_INCLUDE_DIR}
	)
LIST(REMOVE_DUPLICATES IRPREP_INCLUDE_DIRS)
include_directories(${IRPREP_INCLUDE_DIRS})

IF(BRLCAD_ENABLE_X11)
  BRLCAD_ADDEXEC(ir-X ir-X.c "libbu ${X11_LIBRARIES}")
  BRLCAD_ADDEXEC(irdisp irdisp.c "libbu ${X11_LIBRARIES}")
  SET(irprep_X_MANS
    irdisp.1
    ir-X.1
  )
ENDIF(BRLCAD_ENABLE_X11)

BRLCAD_ADDEXEC(all_sf all_sf.c "librt ${M_LIBRARY}")
BRLCAD_ADDEXEC(showtherm showtherm.c "librt ${M_LIBRARY}")
BRLCAD_ADDEXEC(firpass "firpass.c subroutines.c" "librt ${M_LIBRARY}")
BRLCAD_ADDEXEC(secpass "secpass.c subroutines.c" "librt ${M_LIBRARY}")
BRLCAD_ADDEXEC(shapefact shapefact.c "librt ${M_LIBRARY}")

SET(irprep_MANS
  ${irprep_X_MANS}
  firpass.1
  secpass.1
  shapefact.1
  showtherm.1
)
#  pictx.1
ADD_MAN_PAGES(1 irprep_MANS)
CMAKEFILES(pictx.1 pictx.c)
CMAKEFILES(Makefile.am)
