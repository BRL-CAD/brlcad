set(sc_DESCRIPTION "
Option for enabling and disabling compilation of the NIST Step Class
Libraries provided with BRL-CAD's source code.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")

THIRD_PARTY(stepcode SC stepcode sc_DESCRIPTION
  REQUIRED_VARS BRLCAD_LEVEL3 BRLCAD_ENABLE_STEP
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
    set(SC_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
  else (MSVC)
    set(SC_PREFIX "lib")
    set(SC_SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}.${SC_VERSION}")
  endif (MSVC)

  set(SC_DEPS)
  if (TARGET PERPLEX_BLD)
    list(APPEND SC_DEPS perplex_lemon perplex_re2c perplex_perplex)
  endif (TARGET PERPLEX_BLD)

  set(STEPCODE_INSTDIR "${CMAKE_BINARY_DIR}/stepcode")

  ExternalProject_Add(STEPCODE_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/stepcode"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${STEPCODE_INSTDIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
	       -DLEMON_ROOT=${CMAKE_BINARY_DIR}/$<CONFIG>
	       -DRE2C_ROOT=${CMAKE_BINARY_DIR}/$<CONFIG>
	       -DPERPLEX_ROOT=${CMAKE_BINARY_DIR}/$<CONFIG>
               -DSC_IS_SUBBUILD=ON -DSC_PYTHON_GENERATOR=OFF
               -DSC_ENABLE_TESTING=OFF -DSC_ENABLE_COVERAGE=OFF -DSC_BUILD_SCHEMAS=
               -DINCLUDE_INSTALL_DIR=${INCLUDE_DIR}
    DEPENDS ${SC_DEPS}
    )

  # Tell the parent build about files and libraries
  set(STEPCODE_LIBS base express exppp stepcore stepeditor stepdai steputils)
  foreach(SCLIB ${STEPCODE_LIBS})
    ExternalProject_Target(lib${SCLIB} STEPCODE_BLD ${STEPCODE_INSTDIR}
      SHARED ${LIB_DIR}/${SC_PREFIX}${SCLIB}${SC_SUFFIX}
      SYMLINKS ${LIB_DIR}/${SC_PREFIX}${SCLIB}${CMAKE_SHARED_LIBRARY_SUFFIX};${LIB_DIR}/${SC_PREFIX}${SCLIB}${CMAKE_SHARED_LIBRARY_SUFFIX}.2
      LINK_TARGET ${SC_PREFIX}${SCLIB}${CMAKE_SHARED_LIBRARY_SUFFIX}
      RPATH
      )
  endforeach(SCLIB ${STEPCODE_LIBS})
  set(STEPCODE_EXECS check-express exppp exp2cxx)
  foreach(SCEXEC ${STEPCODE_EXECS})
    ExternalProject_Target(${SCEXEC}_exe STEPCODE_BLD ${STEPCODE_INSTDIR}
      EXEC ${BIN_DIR}/${SCEXEC}${CMAKE_EXECUTABLE_SUFFIX}
      RPATH
      )
  endforeach(SCEXEC ${STEPCODE_EXECS})

  ExternalProject_ByProducts(libstepcore_stage STEPCODE_BLD ${STEPCODE_INSTDIR} ${INCLUDE_DIR}/stepcode ${INCLUDE_DIR}/stepcode
    cldai/sdaiApplication_instance_set.h
    cldai/sdaiSession_instance.h
    cldai/sdaiObject.h
    cldai/sdaiString.h
    cldai/sdaiEntity_extent.h
    cldai/sdaiEnum.h
    cldai/sdaiModel_contents.h
    cldai/sdaiBinary.h
    cldai/sdaiEntity_extent_set.h
    cldai/sdaiModel_contents_list.h
    cldai/sdaiDaObject.h
    ordered_attrs.h
    exppp/exppp.h
    express/hash.h
    express/error.h
    express/linklist.h
    express/basic.h
    express/memory.h
    express/lexact.h
    express/type.h
    express/caseitem.h
    express/entity.h
    express/resolve.h
    express/schema.h
    express/stmt.h
    express/expr.h
    express/dict.h
    express/expbasic.h
    express/alg.h
    express/variable.h
    express/express.h
    express/object.h
    express/symbol.h
    express/scope.h
    sc_export.h
    sc_cf.h
    clutils/Str.h
    clutils/gennodearray.h
    clutils/gennode.h
    clutils/errordesc.h
    clutils/gennodelist.h
    clutils/sc_hash.h
    clutils/dirobj.h
    cleditor/cmdmgr.h
    cleditor/editordefines.h
    cleditor/SdaiHeaderSchemaClasses.h
    cleditor/seeinfodefault.h
    cleditor/SdaiHeaderSchema.h
    cleditor/SdaiSchemaInit.h
    cleditor/STEPfile.h
    sc_version_string.h
    sc_stdbool.h
    base/sc_getopt.h
    base/sc_trace_fprintf.h
    base/sc_benchmark.h
    base/sc_memmgr.h
    clstepcore/STEPundefined.h
    clstepcore/mgrnodelist.h
    clstepcore/STEPattribute.h
    clstepcore/STEPaggregate.h
    clstepcore/ExpDict.h
    clstepcore/read_func.h
    clstepcore/needFunc.h
    clstepcore/mgrnodearray.h
    clstepcore/mgrnode.h
    clstepcore/dispnode.h
    clstepcore/sdai.h
    clstepcore/STEPcomplex.h
    clstepcore/instmgr.h
    clstepcore/baseType.h
    clstepcore/sdaiSelect.h
    clstepcore/SubSuperIterators.h
    clstepcore/dictdefs.h
    clstepcore/SingleLinkList.h
    clstepcore/STEPattributeList.h
    clstepcore/dispnodelist.h
    clstepcore/sdaiApplication_instance.h
    clstepcore/Registry.h
    clstepcore/complexSupport.h
    )

  set(STEPCODE_BASE_DIR ${CMAKE_BINARY_DIR}/$<CONFIG>/${INCLUDE_DIR}/stepcode/base CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_DAI_DIR ${CMAKE_BINARY_DIR}/$<CONFIG>/${INCLUDE_DIR}/stepcode/cldai CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_EDITOR_DIR ${CMAKE_BINARY_DIR}/$<CONFIG>/${INCLUDE_DIR}/stepcode/cleditor CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_STEPCORE_DIR ${CMAKE_BINARY_DIR}/$<CONFIG>/${INCLUDE_DIR}/stepcode/clstepcore CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_UTILS_DIR ${CMAKE_BINARY_DIR}/$<CONFIG>/${INCLUDE_DIR}/stepcode/clutils CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_EXPPP_DIR ${CMAKE_BINARY_DIR}/$<CONFIG>/${INCLUDE_DIR}/stepcode/exppp CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_EXPRESS_DIR ${CMAKE_BINARY_DIR}/$<CONFIG>/${INCLUDE_DIR}/stepcode/express CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_INCLUDE_DIR ${CMAKE_BINARY_DIR}/$<CONFIG>/${INCLUDE_DIR}/stepcode CACHE STRING "Building bundled STEPCODE" FORCE)

  set(STEPCODE_BASE_LIBRARY libbase CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_EXPRESS_LIBRARY libexpress CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_EXPPP_LIBRARY libexppp CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_CORE_LIBRARY libstepcore CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_EDITOR_LIBRARY libstepeditor CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_DAI_LIBRARY libstepdai CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_UTILS_LIBRARY libsteputils CACHE STRING "Building bundled STEPCODE" FORCE)

  set(EXP2CXX_EXECUTABLE exp2cxx_exe CACHE STRING "Building bundled STEPCODE" FORCE)
  set(EXPPP_EXECUTABLE exppp_exe CACHE STRING "Building bundled STEPCODE" FORCE)

  set(STEPCODE_INCLUDE_DIRS
    ${STEPCODE_BASE_DIR}
    ${STEPCODE_DAI_DIR}
    ${STEPCODE_EDITOR_DIR}
    ${STEPCODE_STEPCORE_DIR}
    ${STEPCODE_UTILS_DIR}
    ${STEPCODE_EXPPP_DIR}
    ${STEPCODE_EXPRESS_DIR}
    ${STEPCODE_DIR}
    CACHE STRING "Directories containing STEPCODE headers." FORCE)

  set(STEPCODE_LIBRARIES
    ${STEPCODE_BASE_LIBRARY}
    ${STEPCODE_EXPRESS_LIBRARY}
    ${STEPCODE_EXPPP_LIBRARY}
    ${STEPCODE_CORE_LIBRARY}
    ${STEPCODE_EDITOR_LIBRARY}
    ${STEPCODE_DAI_LIBRARY}
    ${STEPCODE_UTILS_LIBRARY}
    CACHE STRING "Directories containing STEPCODE headers." FORCE)

  set(STEPCODE_DIR ${CMAKE_BINARY_DIR}/$<CONFIG>/stepcode CACHE STRING "Building bundled STEPCODE" FORCE)

  SetTargetFolder(STEPCODE_BLD "Third Party Libraries")
  SetTargetFolder(stepcode "Third Party Libraries")
endif(BRLCAD_SC_BUILD)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

