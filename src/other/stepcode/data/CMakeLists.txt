# To build one or more schemas, configure with
# 'cmake -DSC_BUILD_SCHEMAS="path/to/schema.exp;path/to/schema2.exp"

MACRO(RELATIVE_PATH_TO_TOPLEVEL current_dir rel_path)
       string(REPLACE "${SC_SOURCE_DIR}" "" subpath "${current_dir}")
       string(REGEX REPLACE "^/" "" subpath "${subpath}")
       string(LENGTH "${subpath}" PATH_LENGTH)
       if(PATH_LENGTH GREATER 0)
               set(${rel_path} "..")
               get_filename_component(subpath "${subpath}" PATH)
               string(LENGTH "${subpath}" PATH_LENGTH)
               while(PATH_LENGTH GREATER 0)
                       set(${rel_path} "${${rel_path}}/..")
                       get_filename_component(subpath "${subpath}" PATH)
                       string(LENGTH "${subpath}" PATH_LENGTH)
               endwhile(PATH_LENGTH GREATER 0)
       endif(PATH_LENGTH GREATER 0)
ENDMACRO(RELATIVE_PATH_TO_TOPLEVEL current_dir rel_path)

# This function runs fedex on one express file. The generated source goes in a dir
# in the build dir, and it is compiled into a library. A p21read executable is
# compiled and linked to the lib. p21read is used to test the lib.
FUNCTION(BUILD_A_SCHEMA SCHEMA_FILE)
    if( EXISTS "${CMAKE_BINARY_DIR}/${SCHEMA_FILE}" )  #is it a path relative to build dir?
        set( SCHEMA_FILE "${CMAKE_BINARY_DIR}/${SCHEMA_FILE}" )
    elseif( EXISTS "${SC_SOURCE_DIR}/data/${SCHEMA_FILE}" )  # path relative to STEPcode/data?
        set( SCHEMA_FILE "${SC_SOURCE_DIR}/data/${SCHEMA_FILE}" )
    elseif( NOT EXISTS ${SCHEMA_FILE} ) # absolute path
        message( FATAL_ERROR "Cannot find ${CMAKE_BINARY_DIR}/${SCHEMA_FILE} or ${SC_SOURCE_DIR}/data/${SCHEMA_FILE} or ${SCHEMA_FILE}" )
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
    string(TOUPPER ${SCHEMA_N} SCHEMA_LONG_NAME) #exp2cxx always uses upper case for file names
    get_filename_component( SCHEMA_SN ${SCHEMA_FILE} NAME )
    string( REGEX REPLACE "\(.*\).[Ee][Xx][Pp]" "\\1" SCHEMA_SHORT_NAME ${SCHEMA_SN} )

    project( sdai_${SCHEMA_SHORT_NAME} )
    message( STATUS "Generating code for ${SCHEMA_SHORT_NAME}.")
    set( SCHEMA_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/${SCHEMA_SHORT_NAME} )

    #the names of the files that will be generated
    set( FEDEX_OUT ${SCHEMA_OUTPUT_DIR}/compstructs.cc ${SCHEMA_OUTPUT_DIR}/Sdaiclasses.h
                   ${SCHEMA_OUTPUT_DIR}/schema.cc ${SCHEMA_OUTPUT_DIR}/Sdai${SCHEMA_LONG_NAME}.cc
                   ${SCHEMA_OUTPUT_DIR}/schema.h ${SCHEMA_OUTPUT_DIR}/Sdai${SCHEMA_LONG_NAME}.h
                   ${SCHEMA_OUTPUT_DIR}/SdaiAll.cc ${SCHEMA_OUTPUT_DIR}/Sdai${SCHEMA_LONG_NAME}.init.cc )

    # *cannot* use include_directories() because the includes keep piling up - if building
    # multiple schemas, each one will use the include dirs from all previous schemas. Since
    # one header (schema.h) is always named the same, this will not work. only workaround
    # seems to be set_target_properties( <target> PROPERTIES COMPILE_FLAGS <flags> )
    set( ${PROJECT_NAME}_COMPILE_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR} -I${SC_SOURCE_DIR}/src/cldai -I${SC_SOURCE_DIR}/src/cleditor -I${SC_SOURCE_DIR}/src/clutils -I${SCHEMA_OUTPUT_DIR} -I${SC_SOURCE_DIR}/src/clstepcore -I${SC_SOURCE_DIR}/src/base" )

    add_custom_command( OUTPUT ${SCHEMA_OUTPUT_DIR}
                        COMMAND ${CMAKE_COMMAND} ARGS -E make_directory ${SCHEMA_OUTPUT_DIR}
                        COMMENT "Creating ${SCHEMA_OUTPUT_DIR} for schema ${SCHEMA_SHORT_NAME}")
    add_custom_command( OUTPUT ${FEDEX_OUT}
                        COMMAND exp2cxx ARGS ${SCHEMA_FILE}
                        DEPENDS ${SCHEMA_FILE} ${SCHEMA_OUTPUT_DIR}
                        WORKING_DIRECTORY ${SCHEMA_OUTPUT_DIR}
                        COMMENT "Running exp2cxx for ${SCHEMA_SHORT_NAME}..."
                        VERBATIM )
    add_custom_target( generate_cpp_${SCHEMA_SHORT_NAME} DEPENDS ${FEDEX_OUT} SOURCES ${FEDEX_OUT} )

    if( SC_ENABLE_COVERAGE AND NOT SC_COVER_SCHEMAS )
      # Coverage will always be sparse on schema libs. Compiling the libs with the -fprofile-arcs and
      # -ftest-coverage is extremely slow. We can avoid that by temporarily changing build type to release.
      set( PREV_BUILD_TYPE ${CMAKE_BUILD_TYPE} )
      set( CMAKE_BUILD_TYPE "Release" )
    endif( SC_ENABLE_COVERAGE AND NOT SC_COVER_SCHEMAS )

    # if testing is enabled, "TESTABLE" sets property EXCLUDE_FROM_ALL and prevents installation
    SC_ADDLIB( ${PROJECT_NAME} "${FEDEX_OUT}" "stepdai;stepcore;stepeditor;steputils;base" "TESTABLE" )
    add_dependencies( ${PROJECT_NAME} generate_cpp_${SCHEMA_SHORT_NAME} )
    set_target_properties( ${PROJECT_NAME} PROPERTIES COMPILE_FLAGS
                           ${${PROJECT_NAME}_COMPILE_FLAGS} )

    if( SC_ENABLE_COVERAGE AND NOT SC_COVER_SCHEMAS )
      set( CMAKE_BUILD_TYPE ${PREV_BUILD_TYPE} )
    endif( SC_ENABLE_COVERAGE AND NOT SC_COVER_SCHEMAS )

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
   if(NOT WIN32)
    add_test( NAME build_lazy_cpp_${PROJECT_NAME}
              WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
              COMMAND ${CMAKE_COMMAND} --build .
                                       --target lazy_${PROJECT_NAME}
                                       --config $<CONFIGURATION> )
   endif(NOT WIN32)

    # label the tests and set dependencies
    set_tests_properties( generate_cpp_${SCHEMA_SHORT_NAME} PROPERTIES LABELS cpp_schema_gen )
   if(NOT WIN32)
    set_tests_properties( build_lazy_cpp_${PROJECT_NAME} build_cpp_${PROJECT_NAME} PROPERTIES DEPENDS generate_cpp_${SCHEMA_SHORT_NAME} LABELS cpp_schema_build )
   endif(NOT WIN32)

   RELATIVE_PATH_TO_TOPLEVEL(${CMAKE_CURRENT_SOURCE_DIR} RELATIVE_PATH_COMPONENT)
   SC_ADDEXEC( p21read_${PROJECT_NAME} "${RELATIVE_PATH_COMPONENT}/src/test/p21read/p21read.cc" "${PROJECT_NAME};base" "TESTABLE" )
   if(NOT WIN32)
    SC_ADDEXEC( lazy_${PROJECT_NAME} "${RELATIVE_PATH_COMPONENT}/src/cllazyfile/lazy_test.cc" "${PROJECT_NAME};steplazyfile;stepeditor" "TESTABLE" )
   endif(NOT WIN32)
    add_dependencies( p21read_${PROJECT_NAME} version_string )
   if(NOT WIN32)
    add_dependencies( lazy_${PROJECT_NAME} version_string )
   endif(NOT WIN32)
    set_target_properties( p21read_${PROJECT_NAME} PROPERTIES COMPILE_FLAGS
                           ${${PROJECT_NAME}_COMPILE_FLAGS} )
   if(NOT WIN32)
    set_target_properties( lazy_${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "${${PROJECT_NAME}_COMPILE_FLAGS} -I${SC_SOURCE_DIR}/src/base/judy/src" )
   endif(NOT WIN32)


    #add user-defined executables
    foreach( src ${SC_SDAI_ADDITIONAL_EXES_SRCS} )
        get_filename_component( name ${src} NAME_WE )
        get_filename_component( path ${src} ABSOLUTE )
        SC_ADDEXEC( ${name}_${PROJECT_NAME} "${src}" "${PROJECT_NAME};base" "TESTABLE" )
        add_dependencies( ${name}_${PROJECT_NAME} version_string )
        set_target_properties( ${name}_${PROJECT_NAME} PROPERTIES COMPILE_FLAGS
                               "${${PROJECT_NAME}_COMPILE_FLAGS} -I${path}" )
    endforeach( src ${SC_SDAI_ADDITIONAL_EXES_SRCS} )

    #find all part 21 files in schema dir, add a test for each one
    get_filename_component( SCHEMA_DIR ${SCHEMA_FILE} PATH )
    file( GLOB_RECURSE P21_FILES ${SCHEMA_DIR}/*.stp ${SCHEMA_DIR}/*.step ${SCHEMA_DIR}/*.p21 ${SCHEMA_DIR}/*.ifc )
    foreach( TEST_FILE ${P21_FILES} )
    get_filename_component( FNAME ${TEST_FILE} NAME_WE )
        add_test( NAME read_write_cpp_${SCHEMA_SHORT_NAME}_${FNAME}
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                  COMMAND p21read_${PROJECT_NAME} ${TEST_FILE} )
        set_tests_properties( read_write_cpp_${SCHEMA_SHORT_NAME}_${FNAME} PROPERTIES DEPENDS build_cpp_${PROJECT_NAME} LABELS cpp_schema_rw )
       if(NOT WIN32)
        add_test( NAME read_lazy_cpp_${SCHEMA_SHORT_NAME}_${FNAME}
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                  COMMAND lazy_${PROJECT_NAME} ${TEST_FILE} )
        set_tests_properties( read_lazy_cpp_${SCHEMA_SHORT_NAME}_${FNAME} PROPERTIES DEPENDS build_lazy_cpp_${PROJECT_NAME} LABELS cpp_schema_rw )
       endif(NOT WIN32)
    endforeach()

ENDFUNCTION(BUILD_A_SCHEMA)

#----------------------------------------------------------------------------------------------------

if( DEFINED SC_BUILD_SCHEMAS )
    foreach( src ${SC_SDAI_ADDITIONAL_EXES_SRCS} )
        get_filename_component( name ${src} NAME_WE )
        message( STATUS "Additional SDAI executable: ${name}" )
    endforeach( src ${SC_SDAI_ADDITIONAL_EXES_SRCS} )

    if( SC_BUILD_SCHEMAS STREQUAL "ALL" )
        file( GLOB_RECURSE SC_BUILD_SCHEMAS ${SC_SOURCE_DIR}/data/*.exp )
    endif()
    foreach( ap ${SC_BUILD_SCHEMAS} )
        BUILD_A_SCHEMA( ${ap} )
    endforeach()
endif()
