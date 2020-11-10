set(sc_DESCRIPTION "
Option for enabling and disabling compilation of the NIST Step Class
Libraries provided with BRL-CAD's source code.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
set(sc_ALIASES ENABLE_SCL ENABLE_STEP ENABLE_STEP_CLASS_LIBRARIES)

THIRD_PARTY(stepcode SC stepcode sc_DESCRIPTION
  REQUIRED_VARS BRLCAD_LEVEL3 BRLCAD_ENABLE_STEP
  ALIASES ${sc_ALIASES}
  RESET_VARS EXP2CXX_EXEC EXP2CXX_EXECUTABLE_TARGET
  FLAGS NOSYS)

if(BRLCAD_SC_BUILD)

  set(SC_MAJOR_VERSION 3)
  set(SC_MINOR_VERSION 0)
  set(SC_PATCH_VERSION 0)
  set(SC_VERSION ${SC_MAJOR_VERSION}.${SC_MINOR_VERSION}.${SC_PATCH_VERSION})

  if (MSVC)
    set(SC_PREFIX "")
    set(SC_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
  elseif (APPLE)
    set(SC_PREFIX "lib")
    set(SC_SUFFIX ".${SC_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}")
  else (MSVC)
    set(SC_PREFIX "lib")
    set(SC_SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}.${SC_VERSION}")
  endif (MSVC)

  set(SC_DEPS)
  if (TARGET PERPLEX_BLD)
    list(APPEND SC_DEPS perplex_lemon perplex_re2c perplex_perplex)
  endif (TARGET PERPLEX_BLD)

  set(STEPCODE_INSTDIR "${CMAKE_BINARY_INSTALL_ROOT}/stepcode")

  ExternalProject_Add(STEPCODE_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/stepcode"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${STEPCODE_INSTDIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=OFF
	       $<$<NOT:$<BOOL:${CMAKE_CONFIGURATION_TYPES}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
	       -DLEMON_ROOT=${CMAKE_BINARY_ROOT}
	       -DRE2C_ROOT=${CMAKE_BINARY_ROOT}
	       -DPERPLEX_ROOT=${CMAKE_BINARY_ROOT}
               -DSC_IS_SUBBUILD=ON -DSC_PYTHON_GENERATOR=OFF
               -DSC_ENABLE_TESTING=OFF -DSC_ENABLE_COVERAGE=OFF -DSC_BUILD_SCHEMAS=
               -DINCLUDE_INSTALL_DIR=${INCLUDE_DIR}
    DEPENDS ${SC_DEPS}
    )

  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/STEPCODE_BLD-prefix")

  # Tell the parent build about files and libraries
  set(STEPCODE_LIBS express stepcore stepeditor stepdai steputils)
  foreach(SCLIB ${STEPCODE_LIBS})
    set(SYMLINK_1 ${SC_PREFIX}${SCLIB}${CMAKE_SHARED_LIBRARY_SUFFIX})
    if (APPLE)
      set(SYMLINK_2 ${SC_PREFIX}${SCLIB}.${SC_MAJOR_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX})
    else (APPLE)
      set(SYMLINK_2 ${SC_PREFIX}${SCLIB}${CMAKE_SHARED_LIBRARY_SUFFIX}.${SC_MAJOR_VERSION})
    endif (APPLE)
    ExternalProject_Target(SHARED ${SCLIB} STEPCODE_BLD ${STEPCODE_INSTDIR}
      ${SC_PREFIX}${SCLIB}${SC_SUFFIX}
      SYMLINKS ${SYMLINK_1};${SYMLINK_2}
      LINK_TARGET ${SC_PREFIX}${SCLIB}${CMAKE_SHARED_LIBRARY_SUFFIX}
      RPATH
      )
  endforeach(SCLIB ${STEPCODE_LIBS})
  ExternalProject_Target(EXEC exp2cxx_exe STEPCODE_BLD ${STEPCODE_INSTDIR}
    exp2cxx${CMAKE_EXECUTABLE_SUFFIX}
    RPATH
    )
  foreach(SCLIB ${STEPCODE_LIBS})
    add_dependencies(exp2cxx_exe_stage ${SCLIB}_stage)
  endforeach(SCLIB ${STEPCODE_LIBS})

  ExternalProject_ByProducts(stepcore STEPCODE_BLD ${STEPCODE_INSTDIR} ${INCLUDE_DIR}/stepcode
    export.h
    core/baseType.h
    core/complexSupport.h
    core/dictdefs.h
    core/dispnode.h
    core/dispnodelist.h
    core/ExpDict.h
    core/instmgr.h
    core/mgrnodearray.h
    core/mgrnode.h
    core/mgrnodelist.h
    core/needFunc.h
    core/read_func.h
    core/Registry.h
    core/sdaiApplication_instance.h
    core/sdai.h
    core/sdaiSelect.h
    core/SingleLinkList.h
    core/STEPaggregate.h
    core/STEPattribute.h
    core/STEPattributeList.h
    core/STEPcomplex.h
    core/STEPundefined.h
    core/SubSuperIterators.h
    dai/sdaiApplication_instance_set.h
    dai/sdaiBinary.h
    dai/sdaiDaObject.h
    dai/sdaiEntity_extent.h
    dai/sdaiEntity_extent_set.h
    dai/sdaiEnum.h
    dai/sdaiModel_contents.h
    dai/sdaiModel_contents_list.h
    dai/sdaiObject.h
    dai/sdaiSession_instance.h
    dai/sdaiString.h
    editor/STEPfile.h
    editor/SdaiHeaderSchema.h
    editor/SdaiHeaderSchemaClasses.h
    editor/SdaiSchemaInit.h
    editor/cmdmgr.h
    editor/editordefines.h
    editor/seeinfodefault.h
    utils/Str.h
    utils/dirobj.h
    utils/errordesc.h
    utils/gennode.h
    utils/gennodearray.h
    utils/gennodelist.h
    utils/sc_hash.h
    express/alg.h
    express/basic.h
    express/caseitem.h
    express/de_end.h
    express/decstart.h
    express/defstart.h
    express/dict.h
    express/entity.h
    express/error.h
    express/expbasic.h
    express/exppp.h
    express/expr.h
    express/express.h
    express/hash.h
    express/lexact.h
    express/linklist.h
    express/memory.h
    express/object.h
    express/ordered_attrs.h
    express/resolve.h
    express/schema.h
    express/scope.h
    express/stmt.h
    express/symbol.h
    express/type.h
    express/variable.h
    )

  ExternalProject_ByProducts(stepcore STEPCODE_BLD ${STEPCODE_INSTDIR} ${DATA_DIR}/step
    ap203/ap203.exp
    ap203e2/ap203e2_mim_lf.exp
    ap214e3/AP214E3_2010.exp
    ap242/ap242e1.exp
    )

  set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} stepcode  CACHE STRING "Bundled system include dirs" FORCE)

  set(STEPCODE_INCLUDE_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/stepcode CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_INCLUDE_DIRS ${STEPCODE_INCLUDE_DIR} CACHE STRING "Directories containing STEPCODE headers." FORCE)

  set(STEPCODE_EXPRESS_LIBRARY express CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_CORE_LIBRARY stepcore CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_EDITOR_LIBRARY stepeditor CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_DAI_LIBRARY stepdai CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_UTILS_LIBRARY steputils CACHE STRING "Building bundled STEPCODE" FORCE)

  set(EXP2CXX_EXECUTABLE exp2cxx_exe CACHE STRING "Building bundled STEPCODE" FORCE)


  set(STEPCODE_LIBRARIES
    ${STEPCODE_EXPRESS_LIBRARY}
    ${STEPCODE_EXPPP_LIBRARY}
    ${STEPCODE_CORE_LIBRARY}
    ${STEPCODE_EDITOR_LIBRARY}
    ${STEPCODE_DAI_LIBRARY}
    ${STEPCODE_UTILS_LIBRARY}
    CACHE STRING "Directories containing STEPCODE headers." FORCE)

  SetTargetFolder(STEPCODE_BLD "Third Party Libraries")
  SetTargetFolder(stepcode "Third Party Libraries")
endif(BRLCAD_SC_BUILD)

include("${CMAKE_CURRENT_SOURCE_DIR}/stepcode.dist")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

