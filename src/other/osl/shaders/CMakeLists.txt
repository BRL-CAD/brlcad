# FIXME -- Here, we should know in advance which is the OSL_HOME
# because we're building it. For know, just call FindOSL...
include(FindOSL)

message(STATUS "compiling the shaders --------- " ${OSLHOME})

# This macro compiles a OSL shader .osl into .oso using the osl compiler (./oslc)
macro (osl_compile oslsrc objlist headers)

    #message (STATUS "OSL_COMPILE src=${oslsrc}")
    #message (STATUS "  src ${CMAKE_CURRENT_SOURCE_DIR}")
    #message (STATUS "  bin ${CMAKE_CURRENT_BINARY_DIR}")

    # Get the filename (from the full path) without the extension
    GET_FILENAME_COMPONENT( oslsrc_we ${oslsrc} NAME_WE )

    #message(STATUS "OSL RAW = ${oslsrc_we}")

    # Set the destination of the output file (changes .osl by .oso)
    SET(osofile "${CMAKE_CURRENT_BINARY_DIR}/${oslsrc_we}.oso")

    SET( ${objlist} ${${objlist}} ${osofile} )

    #message (STATUS "  obj list now ${${objlist}}")
    #message(STATUS "${OSLHOME}/bin/oslc ${oslsrc}")

    # Compile the shader with ./oslc
    ADD_CUSTOM_COMMAND ( OUTPUT ${osofile}
	COMMAND ${OSLHOME}/bin/oslc ${oslsrc}
	DEPENDS ${${headers}} ${oslsrc} ${CMAKE_CURRENT_SOURCE_DIR}/stdosl.h ${OSLHOME}/bin/oslc
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endmacro ()

FILE(GLOB shader_headers "*.h")
FILE(GLOB shader_source "*.osl")

FOREACH(_shadername ${shader_source})
    osl_compile (${_shadername} shader_objs shader_headers)
ENDFOREACH()

add_custom_target (shaders ALL
    DEPENDS ${shader_objs}
    SOURCES ${shader_source} ${shader_headers})

install (FILES ${shader_headers} ${shader_source} ${shader_objs}
    DESTINATION shaders)
