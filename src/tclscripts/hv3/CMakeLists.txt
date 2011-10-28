SET(hv3_TCLSCRIPTS
	combobox.tcl
	hv3.tcl
	hv3_encodings.tcl
	hv3_form.tcl
	hv3_request.tcl
	hv3_util.tcl
	snit.tcl
)
BRLCAD_ADDDATA(hv3_TCLSCRIPTS tclscripts/hv3)
pkgIndex_BUILD(hv3 tclscripts/hv3)
tclIndex_BUILD(hv3 tclscripts/hv3)
install(FILES hv3.man DESTINATION ${DATA_DIR}/doc)
install(FILES tkhtml.n DESTINATION ${MAN_DIR}/mann)
SET(hv3_ignore_files
	demo
	hv3.man
	license.txt
	license_snit.txt
	tkhtml.n
	pkgIndex.tcl
	tclIndex
	)
CMAKEFILES(${hv3_ignore_files})
CMAKEFILES(Makefile.am)
