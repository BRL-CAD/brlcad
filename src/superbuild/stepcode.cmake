set(sc_DESCRIPTION "
Option for enabling and disabling compilation of the NIST Step Class
Libraries provided with BRL-CAD's source code.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")

THIRD_PARTY(stepcode SC stepcode sc_DESCRIPTION
  REQUIRED_VARS BRLCAD_LEVEL3
  ALIASES ${sc_ALIASES}
  RESET_VARS EXP2CXX_EXEC EXP2CXX_EXECUTABLE_TARGET
  FLAGS NOSYS)

if(BRLCAD_SC_BUILD)

  set(SC_MAJOR_VERSION 2)
  set(SC_MINOR_VERSION 0)
  set(SC_PATCH_VERSION 0)
  set(SC_VERSION ${SC_MAJOR_VERSION}.${SC_MINOR_VERSION}.${SC_PATCH_VERSION})

  if (MSVC)
    set(SC_PREFIX "")
    set(SC_SUFFIX "")
  else (MSVC)
    set(SC_PREFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(SC_SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}.${SC_VERSION}")
  endif (MSVC)


  set(LEMON_TARGET)
  if (TARGET LEMON_BLD)
    set(LEMON_TARGET LEMON_BLD)
    list(APPEND SC_DEPS LEMON_BLD)
  endif (TARGET LEMON_BLD)

  set(RE2C_TARGET)
  if (TARGET RE2C_BLD)
    set(RE2C_TARGET RE2C_BLD)
    list(APPEND SC_DEPS RE2C_BLD)
  endif (TARGET RE2C_BLD)

  set(PERPLEX_TARGET)
  if (TARGET PERPLEX_BLD)
    set(PERPLEX_TARGET PERPLEX_BLD)
    list(APPEND SC_DEPS PERPLEX_BLD)
  endif (TARGET PERPLEX_BLD)

  ExternalProject_Add(STEPCODE_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/stepcode"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
	       -DLEMON_ROOT=$<$<BOOL:${LEMON_TARGET}>:${CMAKE_BINARY_DIR}>
	       -DLEMON_TEMPLATE=$<$<BOOL:${LEMON_TARGET}>:${LEMON_TEMPLATE}>
	       -DRE2C_ROOT=$<$<BOOL:${RE2C_TARGET}>:${CMAKE_BINARY_DIR}>
	       -DPERPLEX_ROOT=$<$<BOOL:${PERPLEX_TARGET}>:${CMAKE_BINARY_DIR}>
               -DSC_IS_SUBBUILD=ON -DSC_PYTHON_GENERATOR=OFF
               -DSC_ENABLE_TESTING=OFF -DSC_ENABLE_COVERAGE=OFF -DSC_BUILD_SCHEMAS=
               -DINCLUDE_INSTALL_DIR=${INCLUDE_DIR}
    DEPENDS ${SC_DEPS}
    )
 set(STEPCODE_LIBS base express exppp stepcore stepeditor stepdai steputils)
  foreach(SCLIB ${STEPCODE_LIBS})
    ExternalProject_Target(lib${SCLIB} STEPCODE_BLD
      OUTPUT_FILE ${SC_PREFIX}${SCLIB}${SC_SUFFIX}
      SYMLINKS "${SC_PREFIX}${SCLIB}${CMAKE_SHARED_LIBRARY_SUFFIX};${SC_PREFIX}${SCLIB}${CMAKE_SHARED_LIBRARY_SUFFIX}.2"
      LINK_TARGET "${SC_PREFIX}${SCLIB}${CMAKE_SHARED_LIBRARY_SUFFIX}"
      RPATH
      )
  endforeach(SCLIB ${STEPCODE_LIBS})
  set(STEPCODE_EXECS check-express exppp exp2cxx)
  foreach(SCEXEC ${STEPCODE_EXECS})
    ExternalProject_Target(${SCEXEC} STEPCODE_BLD
      OUTPUT_FILE ${SCEXEC}${CMAKE_EXECUTABLE_SUFFIX}
      RPATH EXEC
      )
  endforeach(SCEXEC ${STEPCODE_EXECS})

  set(EXP2CXX_EXEC exp2cxx CACHE STRING "Express to C++ executable" FORCE)
  mark_as_advanced(EXP2CXX_EXEC)
  set(EXP2CXX_EXECUTABLE_TARGET exp2cxx CACHE STRING "Express to C++ executable target" FORCE)
  mark_as_advanced(EXP2CXX_EXECUTABLE_TARGET)
  ExternalProject_ByProducts(STEPCODE_BLD ${INCLUDE_DIR}
    stepcode/cldai/sdaiApplication_instance_set.h
    stepcode/cldai/sdaiSession_instance.h
    stepcode/cldai/sdaiObject.h
    stepcode/cldai/sdaiString.h
    stepcode/cldai/sdaiEntity_extent.h
    stepcode/cldai/sdaiEnum.h
    stepcode/cldai/sdaiModel_contents.h
    stepcode/cldai/sdaiBinary.h
    stepcode/cldai/sdaiEntity_extent_set.h
    stepcode/cldai/sdaiModel_contents_list.h
    stepcode/cldai/sdaiDaObject.h
    stepcode/ordered_attrs.h
    stepcode/exppp/exppp.h
    stepcode/express/hash.h
    stepcode/express/error.h
    stepcode/express/linklist.h
    stepcode/express/basic.h
    stepcode/express/memory.h
    stepcode/express/lexact.h
    stepcode/express/type.h
    stepcode/express/caseitem.h
    stepcode/express/entity.h
    stepcode/express/resolve.h
    stepcode/express/schema.h
    stepcode/express/stmt.h
    stepcode/express/expr.h
    stepcode/express/dict.h
    stepcode/express/expbasic.h
    stepcode/express/alg.h
    stepcode/express/variable.h
    stepcode/express/express.h
    stepcode/express/object.h
    stepcode/express/symbol.h
    stepcode/express/scope.h
    stepcode/sc_export.h
    stepcode/sc_cf.h
    stepcode/clutils/Str.h
    stepcode/clutils/gennodearray.h
    stepcode/clutils/gennode.h
    stepcode/clutils/errordesc.h
    stepcode/clutils/gennodelist.h
    stepcode/clutils/sc_hash.h
    stepcode/clutils/dirobj.h
    stepcode/cleditor/cmdmgr.h
    stepcode/cleditor/editordefines.h
    stepcode/cleditor/SdaiHeaderSchemaClasses.h
    stepcode/cleditor/seeinfodefault.h
    stepcode/cleditor/SdaiHeaderSchema.h
    stepcode/cleditor/SdaiSchemaInit.h
    stepcode/cleditor/STEPfile.h
    stepcode/sc_version_string.h
    stepcode/sc_stdbool.h
    stepcode/base/sc_getopt.h
    stepcode/base/sc_trace_fprintf.h
    stepcode/base/sc_benchmark.h
    stepcode/base/sc_memmgr.h
    stepcode/clstepcore/STEPundefined.h
    stepcode/clstepcore/mgrnodelist.h
    stepcode/clstepcore/STEPattribute.h
    stepcode/clstepcore/STEPaggregate.h
    stepcode/clstepcore/ExpDict.h
    stepcode/clstepcore/read_func.h
    stepcode/clstepcore/needFunc.h
    stepcode/clstepcore/mgrnodearray.h
    stepcode/clstepcore/mgrnode.h
    stepcode/clstepcore/dispnode.h
    stepcode/clstepcore/sdai.h
    stepcode/clstepcore/STEPcomplex.h
    stepcode/clstepcore/instmgr.h
    stepcode/clstepcore/baseType.h
    stepcode/clstepcore/sdaiSelect.h
    stepcode/clstepcore/SubSuperIterators.h
    stepcode/clstepcore/dictdefs.h
    stepcode/clstepcore/SingleLinkList.h
    stepcode/clstepcore/STEPattributeList.h
    stepcode/clstepcore/dispnodelist.h
    stepcode/clstepcore/sdaiApplication_instance.h
    stepcode/clstepcore/Registry.h
    stepcode/clstepcore/complexSupport.h
    )

  list(APPEND BRLCAD_DEPS STEPCODE_BLD)

  SetTargetFolder(STEPCODE_BLD "Third Party Libraries")
  SetTargetFolder(stepcode "Third Party Libraries")
endif(BRLCAD_SC_BUILD)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

