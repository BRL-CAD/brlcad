set(boteditor_TCLSCRIPTS botEditor.tcl botPropertyBox.tcl botTools.tcl)
if(BRLCAD_ENABLE_TCL)
  brlcad_adddata(boteditor_TCLSCRIPTS tclscripts/boteditor)
  tclindex_build(tclscripts/boteditor)
endif(BRLCAD_ENABLE_TCL)

cmakefiles(
  CMakeLists.txt
  ${boteditor_TCLSCRIPTS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
