add_definitions(
  -D_CONSOLE
  -DBRLCAD_DLL
  )

BRLCAD_ADDEXEC(asc2dsp asc2dsp.c libbu FOLDER Conv)

# With BRLCAD_ADDEXEC enabling the man page, we need to
# set the man page source property before adding the exec
set_source_files_properties(../../../doc/docbook/system/man1/pix2asc.xml PROPERTIES EXTRA_MAN1_OUTPUTS "asc2pix.1")

BRLCAD_ADDEXEC(asc2pix asc2pix.c libbu FOLDER Conv NO_MAN)

BRLCAD_ADDEXEC(pix2asc pix2asc.c libbu FOLDER Conv)

if (TARGET libtclcad)
  BRLCAD_ADDEXEC(asc2g asc2g.c "libtclcad;libged;libwdb;librt;libnmg;libbu;${WINSOCK_LIB};${M_LIBRARY}" FOLDER Conv)
  BRLCAD_ADDEXEC(g2asc g2asc.c "libtclcad;librt;libnmg;libbu;${WINSOCK_LIB}" FOLDER Conv)
endif (TARGET libtclcad)

CMAKEFILES(
  CMakeLists.txt
  asc2g.c
  g2asc.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
