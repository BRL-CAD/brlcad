include_directories(
		  ${CMAKE_CURRENT_SOURCE_DIR}/src
		  ${CMAKE_CURRENT_BINARY_DIR}
		  ${TCL_INCLUDE_PATH}
		  ${TK_INCLUDE_PATH}
		  ${X11_INCLUDE_DIR}
		  )

SET(TKTABLE_PKGNAME Tktable)
SET(TKTABLE_PKGVERSION "2.10")

ADD_CUSTOM_COMMAND(
		  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tkTable.tcl.h
		  COMMAND ${TCL_TCLSH} ${CMAKE_CURRENT_SOURCE_DIR}/misc/TkTable_header.tcl ${CMAKE_CURRENT_SOURCE_DIR}/library/tkTable.tcl ${CMAKE_CURRENT_BINARY_DIR}/tkTable.tcl.h
)
ADD_CUSTOM_TARGET(tktable_header_gen ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tkTable.tcl.h)

SET(TKTABLE_SRCS
	generic/tkTable.c
	generic/tkTableCell.c
	generic/tkTableCellSort.c
	generic/tkTableCmds.c
	generic/tkTableEdit.c
	#generic/tkTablePs.c
	generic/tkTableTag.c
	generic/tkTableUtil.c
	generic/tkTableWin.c
)

add_definitions(
	-DPACKAGE_NAME=\"${TKTABLE_PKGNAME}\"
	-DPACKAGE_VERSION=\"${TKTABLE_PKGVERSION}\"
	-DSTDC_HEADERS=1
	-DTBL_COMMAND=\"table\" 
	-DTBL_RUNTIME=\"tkTable.tcl\"
	-DTBL_RUNTIME_DIR=\"${CMAKE_INSTALL_PREFIX}/lib/${TKTABLE_PKGNAME}${TKTABLE_PKGVERSION}\"
	)

add_library(Tktable SHARED ${TKTABLE_SRCS})
add_dependencies(Tktable tktable_header_gen)
target_link_libraries(Tktable ${TCL_LIBRARY} ${TK_LIBRARY} ${X11_LIBRARIES})
install(TARGETS Tktable DESTINATION lib)

# Create the pkgIndex.tcl file.
FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl "package ifneeded ${TKTABLE_PKGNAME} ${TKTABLE_PKGVERSION} [list load [file join $dir .. libTktable${CMAKE_SHARED_LIBRARY_SUFFIX}]]")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl DESTINATION lib/${TKTABLE_PKGNAME}${TKTABLE_PKGVERSION})

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.txt DESTINATION lib/${TKTABLE_PKGNAME}${TKTABLE_PKGVERSION})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/license.txt DESTINATION lib/${TKTABLE_PKGNAME}${TKTABLE_PKGVERSION})
