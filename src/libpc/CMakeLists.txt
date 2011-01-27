
# TODO: check existing boost using FindBoost.cmake
#       and remove blind Include.  Longer term, get
#       flex/byacc/bsd m4 working and rewrite boost
#       parsing

include_directories(
	${CMAKE_SOURCE_DIR}/src/other
	${OPENNURBS_INCLUDE_DIR}
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
SET_TARGET_PROPERTIES(libbn PROPERTIES VERSION 19.0.1 SOVERSION 19)

add_executable(solver_test solver_test.cpp)
target_link_libraries(solver_test libpc)

add_executable(vm_test vm_test.cpp)
target_link_libraries(vm_test libpc)
