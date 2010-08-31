set(LIBITCL_PUBLIC_HDRS
    itcl/generic/itcl.h
	 itcl/generic/itclDecls.h
	 itcl/generic/itclInt.h
	 itcl/generic/itclIntDecls.h
)

set(LIBITCL_SOURCES
	itcl/generic/itclStubInit.c 
	itcl/generic/itcl_bicmds.c 
	itcl/generic/itcl_class.c 
	itcl/generic/itcl_cmds.c 
	itcl/generic/itcl_ensemble.c 
	itcl/generic/itcl_linkage.c 
	itcl/generic/itcl_methods.c 
	itcl/generic/itcl_migrate.c 
	itcl/generic/itcl_objects.c 
	itcl/generic/itcl_parse.c 
	itcl/generic/itcl_util.c
)

set(LIBITCLSTUB_SOURCES
	itcl/generic/itclStubLib.c
)

include_directories(
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/src/other/incrTcl/itcl/generic
	${CMAKE_SOURCE_DIR}/src/other/incrTcl/compat/8.4
)

add_library(itcl ${LIBITCL_SOURCES})
target_link_libraries(itcl ${TCL_LIBRARY})
INSTALL(TARGETS itcl LIBRARY DESTINATION ${BRLCAD_INSTALL_LIB_DIR})

add_library(itclstub ${LIBITCLSTUB_SOURCES})
target_link_libraries(itclstub ${TCL_LIBRARY})
INSTALL(TARGETS itclstub LIBRARY DESTINATION ${BRLCAD_INSTALL_LIB_DIR})

if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL)
	install(FILES ${LIBITCL_PUBLIC_HDRS} DESTINATION include)
endif()
