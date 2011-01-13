SET(hv3_TCLSCRIPTS
	combobox.tcl 
	hv3.tcl 
	hv3_encodings.tcl 
	hv3_form.tcl 
	hv3_request.tcl 
	hv3_util.tcl 
	snit.tcl
)
FILE(COPY ${hv3_TCLSCRIPTS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
INSTALL(FILES ${hv3_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/hv3)

install(FILES hv3.man DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/doc)
install(FILES tkhtml.n DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_MAN_DIR}/mann)

pkgIndex_BUILD(hv3 ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/hv3)
tclIndex_BUILD(hv3 ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/hv3)

