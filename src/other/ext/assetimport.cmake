set(assetimport_DESCRIPTION "
Option for enabling and disabling compilation of the Open
Asset Import Library provided with BRL-CAD's source code.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(assetimport ASSETIMPORT assetimport
  assetimport_DESCRIPTION
  REQUIRED_VARS "BRLCAD_ENABLE_ASSETIMPORT;BRLCAD_LEVEL2"
  ALIASES ENABLE_ASSETIMPORT
  RESET_VARS ASSETIMPORT_LIBRARY ASSETIMPORT_LIBRARIES ASSETIMPORT_INCLUDE_DIR ASSETIMPORT_INCLUDE_DIRS 
  )

if (BRLCAD_ASSETIMPORT_BUILD)

  set(ASSETIMPORT_DEPS)
  set(TARGET_LIST zlib)
  foreach(T ${TARGET_LIST})
    if (TARGET ${T}_stage)
      list(APPEND ASSETIMPORT_DEPS ${T}_stage)
    endif (TARGET ${T}_stage)
  endforeach(T ${TARGET_LIST})

  if (TARGET ZLIB_BLD)
    set(ZLIB_TARGET ZLIB_BLD)
  endif (TARGET ZLIB_BLD)

  if (MSVC)
    set(ZLIB_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${ZLIB_BASENAME}.lib)
  elseif (OPENBSD)
    set(ZLIB_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.1.2)
  else (MSVC)
    set(ZLIB_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
  endif (MSVC)

  if (MSVC)
    set(ASSETIMPORT_BASENAME "assimp-vc${MSVC_TOOLSET_VERSION}-mt")
  else (MSVC)
    set(ASSETIMPORT_BASENAME assimp)
  endif (MSVC)
  set_lib_vars(ASSETIMPORT ${ASSETIMPORT_BASENAME} "5" "2" "4")

  set(ASSETIMPORT_INSTDIR ${CMAKE_BINARY_INSTALL_ROOT}/assetimport)

  ExternalProject_Add(ASSETIMPORT_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assetimport"
    BUILD_ALWAYS ${EXT_BUILD_ALWAYS} ${LOG_OPTS}
    CMAKE_ARGS
    $<$<NOT:$<BOOL:${CMAKE_CONFIGURATION_TYPES}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
    -DBIN_DIR=${BIN_DIR}
    -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_INSTALL_PREFIX=${ASSETIMPORT_INSTDIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
    -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=${CMAKE_INSTALL_RPATH_USE_LINK_PATH}
    -DCMAKE_SKIP_BUILD_RPATH=${CMAKE_SKIP_BUILD_RPATH}
    -DLIB_DIR=${LIB_DIR}
    -DCMAKE_INSTALL_LIBDIR=${LIB_DIR}
    -DZLIB_LIBRARY=$<$<BOOL:${ZLIB_TARGET}>:${ZLIB_LIBRARY}>
    -DZLIB_LIBRARIES=$<$<BOOL:${ZLIB_TARGET}>:${ZLIB_LIBRARY}>
    -DZLIB_LIBRARY_DBG=$<$<BOOL:${ZLIB_TARGET}>:${ZLIB_LIBRARY}>
    -DZLIB_LIBRARY_REL=$<$<BOOL:${ZLIB_TARGET}>:${ZLIB_LIBRARY}>
    -DZLIB_ROOT=${CMAKE_BINARY_ROOT}
    -DASSIMP_BUILD_ASSIMP_TOOLS=OFF
    -DASSIMP_BUILD_TESTS=OFF
    -DASSIMP_BUILD_MINIZIP=ON
    -DASSIMP_INJECT_DEBUG_POSTFIX=OFF
    -DASSIMP_INSTALL_PDB=OFF
    DEPENDS ${ASSETIMPORT_DEPS}
    LOG_CONFIGURE ${EXT_BUILD_QUIET}
    LOG_BUILD ${EXT_BUILD_QUIET}
    LOG_INSTALL ${EXT_BUILD_QUIET}
    LOG_OUTPUT_ON_FAILURE ${EXT_BUILD_QUIET}
    )

  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/ASSETIMPORT_BLD-prefix")

  # Tell the parent build about files and libraries
  ExternalProject_Target(SHARED assetimport ASSETIMPORT_BLD ${ASSETIMPORT_INSTDIR}
    ${ASSETIMPORT_BASENAME}${ASSETIMPORT_SUFFIX}
    SYMLINKS ${ASSETIMPORT_SYMLINK_1};${ASSETIMPORT_SYMLINK_2}
    LINK_TARGET ${ASSETIMPORT_SYMLINK_1}
    RPATH
    )

  ExternalProject_ByProducts(assetimport ASSETIMPORT_BLD ${ASSETIMPORT_INSTDIR} ${INCLUDE_DIR}/assimp
    NOINSTALL
    Base64.hpp
    BaseImporter.h
    Bitmap.h
    BlobIOSystem.h
    ByteSwapper.h
    ColladaMetaData.h
    Compiler/poppack1.h
    Compiler/pstdint.h
    Compiler/pushpack1.h
    CreateAnimMesh.h
    DefaultIOStream.h
    DefaultIOSystem.h
    DefaultLogger.hpp
    Exceptional.h
    Exporter.hpp
    GenericProperty.h
    GltfMaterial.h
    Hash.h
    IOStream.hpp
    IOStreamBuffer.h
    IOSystem.hpp
    Importer.hpp
    LineSplitter.h
    LogAux.h
    LogStream.hpp
    Logger.hpp
    MathFunctions.h
    MemoryIOWrapper.h
    NullLogger.hpp
    ObjMaterial.h
    ParsingUtils.h
    Profiler.h
    ProgressHandler.hpp
    RemoveComments.h
    SGSpatialSort.h
    SceneCombiner.h
    SkeletonMeshBuilder.h
    SmallVector.h
    SmoothingGroups.h
    SmoothingGroups.inl
    SpatialSort.h
    StandardShapes.h
    StreamReader.h
    StreamWriter.h
    StringComparison.h
    StringUtils.h
    Subdivision.h
    TinyFormatter.h
    Vertex.h
    XMLTools.h
    XmlParser.h
    ZipArchiveIOSystem.h
    aabb.h
    ai_assert.h
    anim.h
    camera.h
    cexport.h
    cfileio.h
    cimport.h
    color4.h
    color4.inl
    commonMetaData.h
    config.h
    defs.h
    fast_atof.h
    importerdesc.h
    light.h
    material.h
    material.inl
    matrix3x3.h
    matrix3x3.inl
    matrix4x4.h
    matrix4x4.inl
    mesh.h
    metadata.h
    pbrmaterial.h
    postprocess.h
    qnan.h
    quaternion.h
    quaternion.inl
    scene.h
    texture.h
    types.h
    vector2.h
    vector2.inl
    vector3.h
    vector3.inl
    version.h
    )
  set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} assetimport CACHE STRING "Bundled system include dirs" FORCE)

  set(ASSETIMPORT_LIBRARY assetimport CACHE STRING "Building bundled assetimport" FORCE)
  set(ASSETIMPORT_LIBRARIES assetimport CACHE STRING "Building bundled assetimport" FORCE)

  set(ASSETIMPORT_INCLUDE_DIR
    "${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/assetimport"
    "${BRLCAD_SOURCE_DIR}/src/other/ext/assetimport"
    CACHE STRING "Directories containing ASSETIMPORT headers." FORCE)
  set(ASSETIMPORT_INCLUDE_DIRS "${ASSETIMPORT_INCLUDE_DIR}" CACHE STRING "Directories containing ASSETIMPORT headers." FORCE)

  SetTargetFolder(ASSETIMPORT_BLD "Third Party Libraries")
  SetTargetFolder(assetimport "Third Party Libraries")

else (BRLCAD_ASSETIMPORT_BUILD)

  set(ASSETIMPORT_LIBRARIES ${ASSETIMPORT_LIBRARY} CACHE STRING "assetimport" FORCE)
  set(ASSETIMPORT_INCLUDE_DIRS "${ASSETIMPORT_INCLUDE_DIR}" CACHE STRING "Directories containing ASSETIMPORT headers." FORCE)

endif (BRLCAD_ASSETIMPORT_BUILD)

mark_as_advanced(ASSETIMPORT_INCLUDE_DIRS)
mark_as_advanced(ASSETIMPORT_LIBRARIES)

include("${CMAKE_CURRENT_SOURCE_DIR}/assetimport.dist")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

