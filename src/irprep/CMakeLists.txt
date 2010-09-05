include_directories(
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${X11_INCLUDE_DIR}
  ${TCL_INCLUDE_PATH}
)

add_definitions(
  ${BRLCAD_STD_DEFS}
)

IF(BRLCAD-ENABLE_X11)
  BRLCAD_ADDEXEC(ir-X ir-X.c "bu ${X11_LIBRARIES}")
  BRLCAD_ADDEXEC(irdisp irdisp.c "bu ${X11_LIBRARIES}")
ENDIF(BRLCAD-ENABLE_X11)

BRLCAD_ADDEXEC(all_sf all_sf.c librt)
BRLCAD_ADDEXEC(showtherm showtherm.c librt)
BRLCAD_ADDEXEC(firpass "firpass.c subroutines.c" librt)
BRLCAD_ADDEXEC(ir-sgi ir-sgi.c bu)
BRLCAD_ADDEXEC(secpass "secpass.c subroutines.c" librt)
BRLCAD_ADDEXEC(shapefact shapefact.c librt)
