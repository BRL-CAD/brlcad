set(LIBITCL_PUBLIC_HDRS
    itcl/generic/itcl.h
	 itcl/generic/itclDecls.h
	 itcl/generic/itclInt.h
	 itcl/generic/itclIntDecls.h
)

set(LIBITK_PUBLIC_HDRS
    itk/generic/itk.h
	 itk/generic/itkDecls.h
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

set(LIBITK_SOURCES
	itk/generic/itkStubInit.c 
	itk/generic/itk_archetype.c 
	itk/generic/itk_cmds.c 
	itk/generic/itk_option.c 
	itk/generic/itk_util.c
)

set(LIBITKSTUB_SOURCES
	itk/generic/itkStubLib.c
)

IF(TCL_PRIVATE_HDRS AND TK_PRIVATE_HDRS)
	SET(ITCL_COMPAT_HDRS "${TCL_PRIVATE_HDRS}")
	SET(ITK_COMPAT_HDRS "${TCL_PRIVATE_HDRS}")
ELSE(TCL_PRIVATE_HDRS AND TK_PRIVATE_HDRS)
	SET(ITCL_COMPAT_HDRS "${CMAKE_SOURCE_DIR}/src/other/incrTcl/compat/8.4")
	SET(ITK_COMPAT_HDRS "") 
ENDIF(TCL_PRIVATE_HDRS AND TK_PRIVATE_HDRS)

include_directories(
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/src/other/incrTcl/itcl/generic
	${TCL_INCLUDE_PATH}
)

FOREACH(itclincludedir ${ITCL_COMPAT_HDRS})
	include_directories(${itclincludedir})
ENDFOREACH(itclincludedir ${ITCL_COMPAT_HDRS})

FOREACH(itkincludedir ${ITK_COMPAT_HDRS})
	include_directories(${itkincludedir})
ENDFOREACH(itkincludedir ${ITK_COMPAT_HDRS})


add_library(itcl ${LIBITCL_SOURCES})
target_link_libraries(itcl ${TCL_LIBRARY})
INSTALL(TARGETS itcl LIBRARY DESTINATION ${BRLCAD_INSTALL_LIB_DIR})

add_library(itclstub ${LIBITCLSTUB_SOURCES})
target_link_libraries(itclstub ${TCL_LIBRARY})
INSTALL(TARGETS itclstub LIBRARY DESTINATION ${BRLCAD_INSTALL_LIB_DIR})

include_directories(
	${CMAKE_SOURCE_DIR}/src/other/incrTcl/itk/generic
	${CMAKE_SOURCE_DIR}/include
	${TCL_INCLUDE_PATH}
	${TK_INCLUDE_PATH}
)

add_library(itk ${LIBITK_SOURCES})
target_link_libraries(itk itcl ${TCL_LIBRARY} ${TK_LIBRARY})
INSTALL(TARGETS itk LIBRARY DESTINATION ${BRLCAD_INSTALL_LIB_DIR})

add_library(itkstub ${LIBITKSTUB_SOURCES})
target_link_libraries(itkstub ${TCL_LIBRARY} ${TK_LIBRARY})
INSTALL(TARGETS itkstub LIBRARY DESTINATION ${BRLCAD_INSTALL_LIB_DIR})

ADD_DEPENDENCIES(itk itcl itclstub)
ADD_DEPENDENCIES(itkstub itcl itclstub)

if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL)
	install(FILES ${LIBITCL_PUBLIC_HDRS} DESTINATION include)
endif()

if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL)
	install(FILES ${LIBITK_PUBLIC_HDRS} DESTINATION include)
endif()
