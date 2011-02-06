include_directories(
  ${X11_INCLUDE_DIR}
  ${TCL_INCLUDE_DIRS}
)

IF(BRLCAD-ENABLE_X11)
  BRLCAD_ADDEXEC(ir-X ir-X.c "libbu ${X11_LIBRARIES}")
  BRLCAD_ADDEXEC(irdisp irdisp.c "libbu ${X11_LIBRARIES}")
  SET(irprep_X_MANS 
    irdisp.1
    ir-X.1
  )
ENDIF(BRLCAD-ENABLE_X11)

BRLCAD_ADDEXEC(all_sf all_sf.c librt)
BRLCAD_ADDEXEC(showtherm showtherm.c librt)
BRLCAD_ADDEXEC(firpass "firpass.c subroutines.c" librt)
BRLCAD_ADDEXEC(ir-sgi ir-sgi.c libbu)
BRLCAD_ADDEXEC(secpass "secpass.c subroutines.c" librt)
BRLCAD_ADDEXEC(shapefact shapefact.c librt)

SET(irprep_MANS
  ${irprep_X_MANS}
  firpass.1 
  ir-sgi.1 
  secpass.1 
  shapefact.1
  showtherm.1
)
#  pictsgi.1
#  pictx.1
ADD_MAN_PAGES(1 irprep_MANS)
CMAKEFILES(pictsgi.1 pictx.1 Compile.sgi pictsgi.c pictx.c)
