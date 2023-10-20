set(boteditor_TCLSCRIPTS
  botEditor.tcl
  botPropertyBox.tcl
  botTools.tcl
  )
if (BRLCAD_ENABLE_TCL)
  BRLCAD_ADDDATA(boteditor_TCLSCRIPTS tclscripts/boteditor)
  tclIndex_BUILD(tclscripts/boteditor)
endif (BRLCAD_ENABLE_TCL)

CMAKEFILES(
  CMakeLists.txt
  ${boteditor_TCLSCRIPTS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
