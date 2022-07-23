set(assimp_DESCRIPTION "
Option for enabling and disabling compilation of the Open
Asset Import Library provided with BRL-CAD's source code.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(assimp ASSIMP assimp
  assimp_DESCRIPTION
  REQUIRED_VARS "BRLCAD_ENABLE_ASSIMP;BRLCAD_LEVEL2"
  ALIASES ENABLE_ASSIMP
  RESET_VARS ASSIMP_LIBRARY ASSIMP_LIBRARIES ASSIMP_INCLUDE_DIR ASSIMP_INCLUDE_DIRS 
  )

if (BRLCAD_ASSIMP_BUILD)

  set(ASSIMP_DEPS)
  set(TARGET_LIST zlib)
  foreach(T ${TARGET_LIST})
    if (TARGET ${T}_stage)
      list(APPEND ASSIMP_DEPS ${T}_stage)
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
    set(ASSIMP_BASENAME assimp)
    set(ASSIMP_STATICNAME assimp-static)
    set(ASSIMP_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
  elseif (OPENBSD)
    set(ASSIMP_BASENAME libassimp)
    set(ASSIMP_STATICNAME libassimp)
    set(ASSIMP_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.1.0)
  else (MSVC)
    set(ASSIMP_BASENAME libassimp)
    set(ASSIMP_STATICNAME libassimp)
    set(ASSIMP_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
  endif (MSVC)

  set(ASSIMP_INSTDIR ${CMAKE_BINARY_INSTALL_ROOT}/assimp)

  ExternalProject_Add(ASSIMP_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assimp"
    BUILD_ALWAYS ${EXT_BUILD_ALWAYS} ${LOG_OPTS}
    CMAKE_ARGS
    $<$<NOT:$<BOOL:${CMAKE_CONFIGURATION_TYPES}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
    -DBIN_DIR=${BIN_DIR}
    -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_INSTALL_PREFIX=${ASSIMP_INSTDIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
    -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=${CMAKE_INSTALL_RPATH_USE_LINK_PATH}
    -DCMAKE_SKIP_BUILD_RPATH=${CMAKE_SKIP_BUILD_RPATH}
    -DLIB_DIR=${LIB_DIR}
    -DZLIB_LIBRARY=$<$<BOOL:${ZLIB_TARGET}>:${ZLIB_LIBRARY}>
    -DZLIB_ROOT=${CMAKE_BINARY_ROOT}
    -DASSIMP_BUILD_TESTS=OFF
    -DASSIMP_BUILD_MINIZIP=ON
    DEPENDS ${ASSIMP_DEPS}
    LOG_CONFIGURE ${EXT_BUILD_QUIET}
    LOG_BUILD ${EXT_BUILD_QUIET}
    LOG_INSTALL ${EXT_BUILD_QUIET}
    LOG_OUTPUT_ON_FAILURE ${EXT_BUILD_QUIET}
    )

  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/ASSIMP_BLD-prefix")

  # Tell the parent build about files and libraries
  set_lib_vars(ASSIMP assimp "5" "2" "0")
  ExternalProject_Target(SHARED assimp ASSIMP_BLD ${ASSIMP_INSTDIR}
    ${ASSIMP_BASENAME}${ASSIMP_SUFFIX}
    SYMLINKS ${ASSIMP_SYMLINK_1};${ASSIMP_SYMLINK_2}
    LINK_TARGET ${ASSIMP_SYMLINK_1}
    RPATH
    )

  ExternalProject_ByProducts(assimp ASSIMP_BLD ${ASSIMP_INSTDIR} ${INCLUDE_DIR}/assimp
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
  set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} assimp CACHE STRING "Bundled system include dirs" FORCE)

  set(ASSIMP_LIBRARY assimp CACHE STRING "Building bundled assimp" FORCE)
  set(ASSIMP_LIBRARIES assimp CACHE STRING "Building bundled assimp" FORCE)

  set(ASSIMP_INCLUDE_DIR
    "${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/assimp"
    "${BRLCAD_SOURCE_DIR}/src/other/ext/assimp"
    CACHE STRING "Directories containing ASSIMP headers." FORCE)
  set(ASSIMP_INCLUDE_DIRS "${ASSIMP_INCLUDE_DIR}" CACHE STRING "Directories containing ASSIMP headers." FORCE)

  SetTargetFolder(ASSIMP_BLD "Third Party Libraries")
  SetTargetFolder(assimp "Third Party Libraries")

else (BRLCAD_ASSIMP_BUILD)

  set(ASSIMP_LIBRARIES ${ASSIMP_LIBRARY} CACHE STRING "assimp" FORCE)
  set(ASSIMP_INCLUDE_DIRS "${ASSIMP_INCLUDE_DIR}" CACHE STRING "Directories containing ASSIMP headers." FORCE)

endif (BRLCAD_ASSIMP_BUILD)

include("${CMAKE_CURRENT_SOURCE_DIR}/assimp.dist")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

