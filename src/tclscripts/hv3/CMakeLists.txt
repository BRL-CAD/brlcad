set(
  hv3_TCLSCRIPTS
  combobox.tcl
  hv3.tcl
  hv3_encodings.tcl
  hv3_form.tcl
  hv3_request.tcl
  hv3_util.tcl
  snit.tcl
)
if(BRLCAD_ENABLE_TCL)
  brlcad_adddata(hv3_TCLSCRIPTS tclscripts/hv3)
  pkgindex_build(tclscripts/hv3)
  tclindex_build(tclscripts/hv3)
  install(FILES hv3.man DESTINATION ${DATA_DIR}/doc)
  install(FILES tkhtml.n DESTINATION ${MAN_DIR}/mann)
endif(BRLCAD_ENABLE_TCL)

set(
  hv3_ignore_files
  CMakeLists.txt
  demo/small-eagleCAD.gif
  demo/index.html
  demo/viewpage.tcl
  hv3.man
  license.txt
  license_snit.txt
  tkhtml.n
  ${hv3_TCLSCRIPTS}
)
cmakefiles(${hv3_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
