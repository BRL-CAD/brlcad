
#this sets a property so that the target doesn't get built by 'make' or 'make all'
#useful when testing generation of schema code/compiling schem libs/etc
FUNCTION( TESTABLE_TARGET target )
    if(ENABLE_TESTING)
        set_target_properties( ${target} PROPERTIES EXCLUDE_FROM_ALL ON )
    endif()
ENDFUNCTION( TESTABLE_TARGET )

# To build one or more schemas, configure with 
# 'cmake -DSCL_BUILD_SCHEMAS="path/to/schema.exp;path/to/schema2.exp"

# This function runs fedex on one express file. The generated source goes in a dir 
# in the build dir, and it is compiled into a library. A p21read executable is 
# compiled and linked to the lib. p21read is used to test the lib.
FUNCTION(BUILD_A_SCHEMA SCHEMA_FILE)
    if( EXISTS "${CMAKE_BINARY_DIR}/${SCHEMA_FILE}" )  #try absolute path. if that fails, must already be absolute.
        set( SCHEMA_FILE "${CMAKE_BINARY_DIR}/${SCHEMA_FILE}" )
    else()
        if( NOT EXISTS ${SCHEMA_FILE} )
            message( FATAL_ERROR "Cannot find ${CMAKE_BINARY_DIR}/${SCHEMA_FILE} or ${SCHEMA_FILE}")
        endif()
    endif()

    if( IS_DIRECTORY ${SCHEMA_FILE} ) #if it is a dir, look for one .exp file inside
        file(GLOB SCHEMA_FILE ${SCHEMA_FILE}/*.exp )
    endif()

    if( NOT EXISTS ${SCHEMA_FILE} )
        message(FATAL_ERROR "Expected one express file. Found '${SCHEMA_FILE}' instead.")
    endif()

    # read the schema name from a line like 'SCHEMA AUTOMOTIVE_DESIGN;'
    file(STRINGS ${SCHEMA_FILE} SCHEMA_STATEMENT LIMIT_COUNT 1 REGEX "^SCHEMA .*")
    string(REGEX REPLACE "^SCHEMA \(.*\)\;$" "\\1" SCHEMA_N ${SCHEMA_STATEMENT} )
    string(TOUPPER ${SCHEMA_N} SCHEMA_LONG_NAME) #fedex_plus always uses upper case for file names
    get_filename_component( SCHEMA_SN ${SCHEMA_FILE} NAME )
    string( REGEX REPLACE "\(.*\).[Ee][Xx][Pp]" "\\1" SCHEMA_SHORT_NAME ${SCHEMA_SN} )
    
    project( sdai_${SCHEMA_SHORT_NAME} )
    message( STATUS "Generating code for ${SCHEMA_SHORT_NAME}.")
    set( SCHEMA_OUTPUT_DIR ${CMAKE_BINARY_DIR}/${SCHEMA_SHORT_NAME} )

    #the names of the files that will be generated
    set( FEDEX_OUT ${SCHEMA_OUTPUT_DIR}/compstructs.cc ${SCHEMA_OUTPUT_DIR}/Sdaiclasses.h
                   ${SCHEMA_OUTPUT_DIR}/schema.cc ${SCHEMA_OUTPUT_DIR}/Sdai${SCHEMA_LONG_NAME}.cc
                   ${SCHEMA_OUTPUT_DIR}/schema.h ${SCHEMA_OUTPUT_DIR}/Sdai${SCHEMA_LONG_NAME}.h
                   ${SCHEMA_OUTPUT_DIR}/SdaiAll.cc ${SCHEMA_OUTPUT_DIR}/Sdai${SCHEMA_LONG_NAME}.init.cc )

    # *cannot* use include_directories() because the includes keep piling up - if building
    # multiple schemas, each one will use the include dirs from all previous schemas. Since
    # one header (schema.h) is always named the same, this will not work. only workaround
    # seems to be set_target_properties( <target> PROPERTIES COMPILE_FLAGS <flags> )
    set( ${PROJECT_NAME}_COMPILE_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR} -I${SCL_SOURCE_DIR}/src/cldai -I${SCL_SOURCE_DIR}/src/cleditor -I${SCL_SOURCE_DIR}/src/clutils -I${SCHEMA_OUTPUT_DIR} -I${SCL_SOURCE_DIR}/src/clstepcore" )

    add_custom_target( generate_cpp_${SCHEMA_SHORT_NAME} SOURCES ${FEDEX_OUT} )
    add_custom_command( OUTPUT ${SCHEMA_OUTPUT_DIR}
                        COMMAND cmake ARGS -E make_directory ${SCHEMA_OUTPUT_DIR}
                        COMMENT "Creating ${SCHEMA_OUTPUT_DIR} for schema ${SCHEMA_SHORT_NAME}")
    add_custom_command( OUTPUT ${FEDEX_OUT}
                        COMMAND fedex_plus ARGS ${SCHEMA_FILE}
                        DEPENDS ${SCHEMA_FILE} ${SCHEMA_OUTPUT_DIR}
                        WORKING_DIRECTORY ${SCHEMA_OUTPUT_DIR}
                        COMMENT "Running fedex_plus for ${SCHEMA_SHORT_NAME}..."
                        VERBATIM )
    if(MSVC OR BORLAND)
        add_definitions( -DSCL_SCHEMA_DLL_EXPORTS )
        add_definitions( -DSCL_EXPRESS_DLL_IMPORTS )
		add_definitions( -DSCL_UTILS_DLL_IMPORTS )
        add_definitions( -DSCL_DAI_DLL_IMPORTS )
        add_definitions( -DSCL_CORE_DLL_IMPORTS )
        add_definitions( -DSCL_EDITOR_DLL_IMPORTS )
    endif()
    add_library( ${PROJECT_NAME} SHARED ${FEDEX_OUT} )
    target_link_libraries(${PROJECT_NAME} stepdai stepcore express stepeditor steputils )
    add_dependencies( ${PROJECT_NAME} generate_cpp_${SCHEMA_SHORT_NAME} )
    set_target_properties( ${PROJECT_NAME} PROPERTIES COMPILE_FLAGS
                           ${${PROJECT_NAME}_COMPILE_FLAGS} )
    if(APPLE)
        set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-flat_namespace -undefined suppress")
    endif(APPLE)
    TESTABLE_TARGET( ${PROJECT_NAME} )

    add_test( NAME generate_cpp_${SCHEMA_SHORT_NAME}
              WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
              COMMAND ${CMAKE_COMMAND} --build .
                                       --target generate_cpp_${SCHEMA_SHORT_NAME}
                                       --config $<CONFIGURATION> )
    add_test( NAME build_cpp_${PROJECT_NAME}
              WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
              COMMAND ${CMAKE_COMMAND} --build .
                                       --target p21read_${PROJECT_NAME}
                                       --config $<CONFIGURATION> )

    # label the tests and set dependencies
    set_tests_properties( generate_cpp_${SCHEMA_SHORT_NAME} PROPERTIES LABELS cpp_schema_gen )
    set_tests_properties( build_cpp_${PROJECT_NAME} PROPERTIES DEPENDS generate_cpp_${SCHEMA_SHORT_NAME} LABELS cpp_schema_build )

    if(MSVC OR BORLAND)
        add_executable( p21read_${PROJECT_NAME} ${SCL_SOURCE_DIR}/src/test/p21read/p21read.cc ${SCL_SOURCE_DIR}/src/fedex_plus/xgetopt.cc )
    else()
        add_executable( p21read_${PROJECT_NAME} ${SCL_SOURCE_DIR}/src/test/p21read/p21read.cc )
    endif()
    add_dependencies( p21read_${PROJECT_NAME} version_string )
    target_link_libraries( p21read_${PROJECT_NAME} ${PROJECT_NAME} )
    set_target_properties( p21read_${PROJECT_NAME} PROPERTIES COMPILE_FLAGS
                           ${${PROJECT_NAME}_COMPILE_FLAGS} )
    TESTABLE_TARGET( p21read_${PROJECT_NAME} )

    #find all part 21 files in schema dir, add a test for each one
    get_filename_component( SCHEMA_DIR ${SCHEMA_FILE} PATH )
    file( GLOB_RECURSE P21_FILES ${SCHEMA_DIR}/*.stp ${SCHEMA_DIR}/*.step ${SCHEMA_DIR}/*.p21 )
    foreach( TEST_FILE ${P21_FILES} )
    get_filename_component( FNAME ${TEST_FILE} NAME_WE )
        add_test( NAME read_write_cpp_${SCHEMA_SHORT_NAME}_${FNAME}
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                  COMMAND p21read_${PROJECT_NAME} ${TEST_FILE} )
        set_tests_properties( read_write_cpp_${SCHEMA_SHORT_NAME}_${FNAME} PROPERTIES DEPENDS build_cpp_${PROJECT_NAME} LABELS cpp_schema_rw )
    endforeach()

ENDFUNCTION(BUILD_A_SCHEMA)

if( DEFINED SCL_BUILD_SCHEMAS )
    if( SCL_BUILD_SCHEMAS STREQUAL "ALL" )
        file( GLOB_RECURSE SCL_BUILD_SCHEMAS ${SCL_SOURCE_DIR}/data/*.exp )
    endif()
    foreach( ap ${SCL_BUILD_SCHEMAS} )
        BUILD_A_SCHEMA( ${ap} )
    endforeach()
endif()
