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

  set(SC_MAJOR_VERSION 0)
  set(SC_MINOR_VERSION 9)
  set(SC_PATCH_VERSION 1)
  set(SC_VERSION ${SC_MAJOR_VERSION}.${SC_MINOR_VERSION}.${SC_PATCH_VERSION})

  set_lib_vars(REGEX regex_brl "1" "0" "4")

  if (MSVC)
    set(SC_PREFIX "")
    set(SC_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
  elseif (APPLE)
    set(SC_PREFIX "lib")
    set(SC_SUFFIX ".${SC_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}")
  elseif (OPENBSD)
    set(SC_PREFIX "lib")
    set(SC_SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}.${SC_MAJOR_VERSION}.${SC_MINOR_VERSION}")
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
    BUILD_ALWAYS ${EXT_BUILD_ALWAYS} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${STEPCODE_INSTDIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_SKIP_BUILD_RPATH=${CMAKE_SKIP_BUILD_RPATH}
               -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=${CMAKE_INSTALL_RPATH_USE_LINK_PATH}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=OFF
	       $<$<NOT:$<BOOL:${CMAKE_CONFIGURATION_TYPES}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
	       -DLEMON_ROOT=${CMAKE_BINARY_ROOT}
	       -DLEMON_TEMPLATE=${CMAKE_BINARY_ROOT}/${DATA_DIR}/lemon/lempar.c
	       -DRE2C_ROOT=${CMAKE_BINARY_ROOT}
	       -DPERPLEX_ROOT=${CMAKE_BINARY_ROOT}
               -DSC_IS_SUBBUILD=ON -DSC_PYTHON_GENERATOR=OFF
               -DSC_ENABLE_TESTING=OFF -DSC_ENABLE_COVERAGE=OFF -DSC_BUILD_SCHEMAS=
               -DINCLUDE_INSTALL_DIR=${INCLUDE_DIR}
    DEPENDS ${SC_DEPS}
    )

  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/STEPCODE_BLD-prefix")

  # Tell the parent build about files and libraries
  set(STEPCODE_LIBS base express stepcore stepeditor stepdai steputils)
  foreach(SCLIB ${STEPCODE_LIBS})
    set_lib_vars(SC ${SCLIB} "${SC_MAJOR_VERSION}" "${SC_MINOR_VERSION}" "${SC_PATCH_VERSION}")
    ExternalProject_Target(SHARED ${SCLIB} STEPCODE_BLD ${STEPCODE_INSTDIR}
      ${SC_BASENAME}${SC_SUFFIX}
      SYMLINKS ${SC_SYMLINK_1};${SC_SYMLINK_2}
      LINK_TARGET ${SYMLINK_1}
      RPATH
      )
  endforeach(SCLIB ${STEPCODE_LIBS})
  # libexppp is a special naming case, to avoid conflict with the exppp executable
  set_lib_vars(SC exppp "${SC_MAJOR_VERSION}" "${SC_MINOR_VERSION}" "${SC_PATCH_VERSION}")
  if (MSVC)
    set(SC_BASENAME "libexppp")
  endif (MSVC)
  ExternalProject_Target(SHARED libexppp STEPCODE_BLD ${STEPCODE_INSTDIR}
    ${SC_BASENAME}${SC_SUFFIX}
    SYMLINKS ${SC_SYMLINK_1};${SC_SYMLINK_2}
    LINK_TARGET ${SC_SYMLINK_1}
    RPATH
    )
  set(STEPCODE_EXECS check-express exppp exp2cxx)
  foreach(SCEXEC ${STEPCODE_EXECS})
    ExternalProject_Target(EXEC ${SCEXEC}_exe STEPCODE_BLD ${STEPCODE_INSTDIR}
      ${SCEXEC}${CMAKE_EXECUTABLE_SUFFIX}
      RPATH
      )
    foreach(SCLIB ${STEPCODE_LIBS})
      add_dependencies(${SCEXEC}_exe_stage ${SCLIB}_stage)
    endforeach(SCLIB ${STEPCODE_LIBS})
    add_dependencies(${SCEXEC}_exe_stage libexppp_stage)
  endforeach(SCEXEC ${STEPCODE_EXECS})

  ExternalProject_ByProducts(stepcore STEPCODE_BLD ${STEPCODE_INSTDIR} ${INCLUDE_DIR}/stepcode
    base/judy.h
    base/judyL2Array.h
    base/judyLArray.h
    base/judyS2Array.h
    base/judySArray.h
    base/path2str.h
    base/sc_benchmark.h
    base/sc_getopt.h
    base/sc_memmgr.h
    base/sc_mkdir.h
    base/sc_nullptr.h
    base/sc_trace_fprintf.h
    cldai/sdaiApplication_instance_set.h
    cldai/sdaiBinary.h
    cldai/sdaiDaObject.h
    cldai/sdaiEntity_extent.h
    cldai/sdaiEntity_extent_set.h
    cldai/sdaiEnum.h
    cldai/sdaiModel_contents.h
    cldai/sdaiModel_contents_list.h
    cldai/sdaiObject.h
    cldai/sdaiSession_instance.h
    cldai/sdaiString.h
    cleditor/STEPfile.h
    cleditor/SdaiHeaderSchema.h
    cleditor/SdaiHeaderSchemaClasses.h
    cleditor/SdaiSchemaInit.h
    cleditor/cmdmgr.h
    cleditor/editordefines.h
    cleditor/seeinfodefault.h
    cllazyfile/headerSectionReader.h
    cllazyfile/instMgrHelper.h
    cllazyfile/lazyDataSectionReader.h
    cllazyfile/lazyFileReader.h
    cllazyfile/lazyInstMgr.h
    cllazyfile/lazyP21DataSectionReader.h
    cllazyfile/lazyTypes.h
    cllazyfile/p21HeaderSectionReader.h
    cllazyfile/sectionReader.h
    clstepcore/ExpDict.h
    clstepcore/Registry.h
    clstepcore/STEPaggrBinary.h
    clstepcore/STEPaggrEntity.h
    clstepcore/STEPaggrEnum.h
    clstepcore/STEPaggrGeneric.h
    clstepcore/STEPaggrInt.h
    clstepcore/STEPaggrReal.h
    clstepcore/STEPaggrSelect.h
    clstepcore/STEPaggrString.h
    clstepcore/STEPaggregate.h
    clstepcore/STEPattribute.h
    clstepcore/STEPattributeList.h
    clstepcore/STEPcomplex.h
    clstepcore/STEPinvAttrList.h
    clstepcore/STEPundefined.h
    clstepcore/SingleLinkList.h
    clstepcore/SubSuperIterators.h
    clstepcore/aggrTypeDescriptor.h
    clstepcore/attrDescriptor.h
    clstepcore/attrDescriptorList.h
    clstepcore/baseType.h
    clstepcore/complexSupport.h
    clstepcore/create_Aggr.h
    clstepcore/derivedAttribute.h
    clstepcore/dictSchema.h
    clstepcore/dictdefs.h
    clstepcore/dictionaryInstance.h
    clstepcore/dispnode.h
    clstepcore/dispnodelist.h
    clstepcore/entityDescriptor.h
    clstepcore/entityDescriptorList.h
    clstepcore/enumTypeDescriptor.h
    clstepcore/explicitItemId.h
    clstepcore/globalRule.h
    clstepcore/implicitItemId.h
    clstepcore/instmgr.h
    clstepcore/interfaceSpec.h
    clstepcore/interfacedItem.h
    clstepcore/inverseAttribute.h
    clstepcore/inverseAttributeList.h
    clstepcore/mgrnode.h
    clstepcore/mgrnodearray.h
    clstepcore/mgrnodelist.h
    clstepcore/needFunc.h
    clstepcore/read_func.h
    clstepcore/realTypeDescriptor.h
    clstepcore/schRename.h
    clstepcore/sdai.h
    clstepcore/sdaiApplication_instance.h
    clstepcore/sdaiSelect.h
    clstepcore/selectTypeDescriptor.h
    clstepcore/stringTypeDescriptor.h
    clstepcore/typeDescriptor.h
    clstepcore/typeDescriptorList.h
    clstepcore/typeOrRuleVar.h
    clstepcore/uniquenessRule.h
    clstepcore/whereRule.h
    clutils/Str.h
    clutils/dirobj.h
    clutils/errordesc.h
    clutils/gennode.h
    clutils/gennodearray.h
    clutils/gennodelist.h
    clutils/sc_hash.h
    exppp/exppp.h
    express/alg.h
    express/basic.h
    express/caseitem.h
    express/dict.h
    express/entity.h
    express/error.h
    express/expbasic.h
    express/expr.h
    express/express.h
    express/hash.h
    express/lexact.h
    express/linklist.h
    express/memory.h
    express/object.h
    express/resolve.h
    express/schema.h
    express/scope.h
    express/stmt.h
    express/symbol.h
    express/type.h
    express/variable.h
    ordered_attrs.h
    sc_cf.h
    sc_export.h
    sc_stdbool.h
    sc_version_string.h
    )

  set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} stepcode  CACHE STRING "Bundled system include dirs" FORCE)

  set(STEPCODE_BASE_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/stepcode/base CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_DAI_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/stepcode/cldai CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_EDITOR_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/stepcode/cleditor CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_STEPCORE_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/stepcode/clstepcore CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_UTILS_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/stepcode/clutils CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_EXPPP_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/stepcode/exppp CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_EXPRESS_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/stepcode/express CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_INCLUDE_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/stepcode CACHE STRING "Building bundled STEPCODE" FORCE)

  set(STEPCODE_BASE_LIBRARY base CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_EXPRESS_LIBRARY express CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_EXPPP_LIBRARY exppp CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_CORE_LIBRARY stepcore CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_EDITOR_LIBRARY stepeditor CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_DAI_LIBRARY stepdai CACHE STRING "Building bundled STEPCODE" FORCE)
  set(STEPCODE_UTILS_LIBRARY steputils CACHE STRING "Building bundled STEPCODE" FORCE)

  set(EXP2CXX_EXECUTABLE exp2cxx_exe CACHE STRING "Building bundled STEPCODE" FORCE)
  set(EXPPP_EXECUTABLE exppp_exe CACHE STRING "Building bundled STEPCODE" FORCE)

  set(STEPCODE_INCLUDE_DIRS
    ${STEPCODE_DIR}
    ${STEPCODE_BASE_DIR}
    ${STEPCODE_STEPCORE_DIR}
    ${STEPCODE_EDITOR_DIR}
    ${STEPCODE_UTILS_DIR}
    ${STEPCODE_DAI_DIR}
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

  set(STEPCODE_DIR ${CMAKE_BINARY_ROOT}/ext/stepcode CACHE STRING "Building bundled STEPCODE" FORCE)

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

