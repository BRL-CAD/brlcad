
# TODO: check existing boost using FindBoost.cmake
#       and remove blind Include.  Longer term, use
#	re2c/lemon to rewrite parsing code currently
#       using boost

# libpc compilation produces excessive warnings about unquellable
# inline failures due to internal boost header logic.  turn off inline
# warnings outright.
string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE)
if(BUILD_TYPE)
  set(CMAKE_CXX_FLAGS_${BUILD_TYPE} "${CMAKE_CXX_FLAGS_${BUILD_TYPE}} -Wno-inline -Wno-error")
else(BUILD_TYPE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-inline -Wno-error")
endif(BUILD_TYPE)

# Include directories needed by libpc users
set(PC_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${OPENNURBS_INCLUDE_DIR}
  ${ZLIB_INCLUDE_DIR}
  )

set(PC_LOCAL_INCLUDE_DIRS ${BRLCAD_BOOST_INCLUDE_DIR})

BRLCAD_LIB_INCLUDE_DIRS(pc PC_INCLUDE_DIRS PC_LOCAL_INCLUDE_DIRS)

set(LIBPC_NOINST_HDRS
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

set(LIBPC_SOURCES
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

BRLCAD_ADDLIB(libpc "${LIBPC_SOURCES}" "libbu;${OPENNURBS_LIBRARY};${STDCXX_LIBRARIES}" NO_STRICT_CXX)
SET_TARGET_PROPERTIES(libpc PROPERTIES VERSION 20.0.1 SOVERSION 20)

#BRLCAD_ADDEXEC(solver_test solver_test.cpp libpc NO_INSTALL)
#BRLCAD_ADDEXEC(vm_test vm_test.cpp libpc NO_INSTALL)

set(pc_ignore_files
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

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
