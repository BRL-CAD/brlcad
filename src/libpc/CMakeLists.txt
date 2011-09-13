
# TODO: check existing boost using FindBoost.cmake
#       and remove blind Include.  Longer term, get
#       flex/byacc/bsd m4 working and rewrite boost
#       parsing

# No point in warnings here, we can't do anything about
# it until the boost issue is handled
STRING(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE)
IF(BUILD_TYPE)
	SET(CMAKE_CXX_FLAGS_${BUILD_TYPE} "${CMAKE_CXX_FLAGS_${BUILD_TYPE}} -w")
ELSE(BUILD_TYPE)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
ENDIF(BUILD_TYPE)

include_directories(
	${CMAKE_SOURCE_DIR}/src/other/boost
	${OPENNURBS_INCLUDE_DIR}
	${TCL_INCLUDE_DIRS}
	${ZLIB_INCLUDE_DIR}
	)

SET(LIBPC_NOINST_HDRS
	pcBasic.h
	pcInterval.h
	pcVariable.h
	pcParameter.h
	pcConstraint.h
	pcVCSet.h
	pcNetwork.h
	pcGenerator.h
	pcParser.h
	pcMathVM.h
	pcMathLF.h
	pcMathGrammar.h
	pcSolver.h
	)

SET(LIBPC_SOURCES
	pc_main.c
	pc_constraints.c
	pcVariable.cpp
	pcParameter.cpp
	pcConstraint.cpp
	pcVCSet.cpp
	pcNetwork.cpp
	pcGenerator.cpp
	pcParser.cpp
	pcMathVM.cpp
	)

add_definitions(
	-DOBJ_BREP=1
	)

BRLCAD_ADDLIB(libpc "${LIBPC_SOURCES}" libbu ${OPENNURBS_LIBRARY})
SET_TARGET_PROPERTIES(libpc PROPERTIES VERSION 19.0.1 SOVERSION 19)

#add_executable(solver_test solver_test.cpp)
#target_link_libraries(solver_test libpc)

#add_executable(vm_test vm_test.cpp)
#target_link_libraries(vm_test libpc)

SET(pc_ignore_files
	NOTES
	TODO
	pcBasic.h
	pcConstraint.h
	pcGenerator.h
	pcInterval.h
	pcMathGrammar.h
	pcMathLF.h
	pcMathVM.h
	pcNetwork.h
	pcParameter.h
	pcParser.h
	pcSolver.h
	pcVCSet.h
	pcVariable.h
	solver_test.cpp
	vm_test.cpp
	)
CMAKEFILES(${pc_ignore_files})
CMAKEFILES(Makefile.am)
