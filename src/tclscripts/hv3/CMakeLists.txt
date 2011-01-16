SET(hv3_TCLSCRIPTS
	combobox.tcl 
	hv3.tcl 
	hv3_encodings.tcl 
	hv3_form.tcl 
	hv3_request.tcl 
	hv3_util.tcl 
	snit.tcl
)
FILE(COPY ${hv3_TCLSCRIPTS} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/tclscripts/hv3)
INSTALL(FILES ${hv3_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/hv3)

install(FILES hv3.man DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/doc)
install(FILES tkhtml.n DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_MAN_DIR}/mann)

pkgIndex_BUILD(hv3 tclscripts/hv3)
tclIndex_BUILD(hv3 tclscripts/hv3)

