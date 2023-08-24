set(openmesh_DESCRIPTION "
Option for enabling and disabling compilation of the OpenMesh 
Library provided with BRL-CAD's source code.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(openmesh OPENMESH openmesh
  openmesh_DESCRIPTION
  FIND_NAME OpenMesh
  REQUIRED_VARS "BRLCAD_ENABLE_OPENMESH;BRLCAD_LEVEL2"
  ALIASES ENABLE_OPENMESH
  RESET_VARS OPENMESH_LIBRARY OPENMESH_LIBRARIES OPENMESH_INCLUDE_DIR OPENMESH_INCLUDE_DIRS 
  )

if (BRLCAD_OPENMESH_BUILD)
  set(OM_MAJOR_VERSION 9)
  set(OM_MINOR_VERSION 0)
  set(OM_PATCH_VERSION 0)

  if (MSVC)
    set(OPENMESH_BASENAME OpenMesh)
    set(OPENMESH_STATICNAME OpenMesh-static)
    set(OPENMESH_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
  elseif (APPLE)
    set(OPENMESH_BASENAME libOpenMesh)
    set(OPENMESH_STATICNAME libOpenMesh)
    set(OPENMESH_SUFFIX .${OM_MAJOR_VERSION}.${OM_MINOR_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX})
  elseif (OPENBSD)
    set(OPENMESH_BASENAME libOpenMeshCore)
    set(OPENMESH_STATICNAME libOpenMeshCore)
    set(OPENMESH_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.1.0)
  else (MSVC)
    set(OPENMESH_BASENAME libOpenMesh)
    set(OPENMESH_STATICNAME libOpenMesh)
    set(OPENMESH_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${OM_MAJOR_VERSION}.${OM_MINOR_VERSION})
  endif (MSVC)

  set(OPENMESH_INSTDIR ${CMAKE_BINARY_INSTALL_ROOT}/openmesh)

  ExternalProject_Add(OPENMESH_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/openmesh"
    BUILD_ALWAYS ${EXT_BUILD_ALWAYS} ${LOG_OPTS}
    CMAKE_ARGS
    $<$<NOT:$<BOOL:${CMAKE_CONFIGURATION_TYPES}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
    -DBIN_DIR=${BIN_DIR}
    -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_INSTALL_PREFIX=${OPENMESH_INSTDIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
    -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=${CMAKE_INSTALL_RPATH_USE_LINK_PATH}
    -DCMAKE_SKIP_BUILD_RPATH=${CMAKE_SKIP_BUILD_RPATH}
    -DLIB_DIR=${LIB_DIR}
    -DBUILD_APPS=OFF
    -DOPENMESH_BUILD_SHARED=ON
    LOG_CONFIGURE ${EXT_BUILD_QUIET}
    LOG_BUILD ${EXT_BUILD_QUIET}
    LOG_INSTALL ${EXT_BUILD_QUIET}
    LOG_OUTPUT_ON_FAILURE ${EXT_BUILD_QUIET}
    )

  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/OPENMESH_BLD-prefix")

  # Tell the parent build about files and libraries
  set(OPENMESH_LIBS Core Tools)
  foreach(OMLIB ${OPENMESH_LIBS})
    ExternalProject_Target(SHARED ${OMLIB} OPENMESH_BLD ${OPENMESH_INSTDIR}
    ${OPENMESH_BASENAME}${OMLIB}${OPENMESH_SUFFIX}
    SYMLINKS ${OPENMESH_BASENAME}${OMLIB}${CMAKE_SHARED_LIBRARY_SUFFIX}
    LINK_TARGET ${OPENMESH_BASENAME}${OMLIB}${CMAKE_SHARED_LIBRARY_SUFFIX}
    RPATH
    )
  endforeach(OMLIB ${OPENMESH_LIBS})

  set(OPENMESH_HDRS
    NOINSTALL
    Core/Geometry/Config.hh
    Core/Geometry/LoopSchemeMaskT.hh
    Core/Geometry/NormalConeT.hh
    Core/Geometry/Plane3d.hh
    Core/Geometry/Vector11T.hh
    Core/Geometry/VectorT_inc.hh
    Core/Geometry/EigenVectorT.hh
    Core/Geometry/MathDefs.hh
    Core/Geometry/NormalConeT_impl.hh
    Core/Geometry/QuadricT.hh
    Core/Geometry/VectorT.hh
    Core/IO/BinaryHelper.hh
    Core/IO/exporter/BaseExporter.hh
    Core/IO/exporter/ExporterT.hh
    Core/IO/importer/BaseImporter.hh
    Core/IO/importer/ImporterT.hh
    Core/IO/IOInstances.hh
    Core/IO/IOManager.hh
    Core/IO/MeshIO.hh
    Core/IO/OFFFormat.hh
    Core/IO/OMFormat.hh
    Core/IO/OMFormatT_impl.hh
    Core/IO/Options.hh
    Core/IO/reader/BaseReader.hh
    Core/IO/reader/OBJReader.hh
    Core/IO/reader/OFFReader.hh
    Core/IO/reader/OMReader.hh
    Core/IO/reader/PLYReader.hh
    Core/IO/reader/STLReader.hh
    Core/IO/SR_binary.hh
    Core/IO/SR_binary_spec.hh
    Core/IO/SR_binary_vector_of_bool.inl
    Core/IO/SR_rbo.hh
    Core/IO/SR_store.hh
    Core/IO/SR_types.hh
    Core/IO/StoreRestore.hh
    Core/IO/writer/BaseWriter.hh
    Core/IO/writer/OBJWriter.hh
    Core/IO/writer/OFFWriter.hh
    Core/IO/writer/OMWriter.hh
    Core/IO/writer/PLYWriter.hh
    Core/IO/writer/STLWriter.hh
    Core/IO/writer/VTKWriter.hh
    Core/Mesh/ArrayItems.hh
    Core/Mesh/ArrayKernel.hh
    Core/Mesh/ArrayKernelT_impl.hh
    Core/Mesh/AttribKernelT.hh
    Core/Mesh/Attributes.hh
    Core/Mesh/BaseKernel.hh
    Core/Mesh/BaseMesh.hh
    Core/Mesh/Casts.hh
    Core/Mesh/CirculatorsT.hh
    Core/Mesh/DefaultPolyMesh.hh
    Core/Mesh/DefaultTriMesh.hh
    Core/Mesh/FinalMeshItemsT.hh
    Core/Mesh/gen/circulators_header.hh
    Core/Mesh/gen/circulators_template.hh
    Core/Mesh/gen/footer.hh
    Core/Mesh/gen/iterators_header.hh
    Core/Mesh/gen/iterators_template.hh
    Core/Mesh/Handles.hh
    Core/Mesh/IteratorsT.hh
    Core/Mesh/PolyConnectivity.hh
    Core/Mesh/PolyConnectivity_inline_impl.hh
    Core/Mesh/PolyMesh_ArrayKernelT.hh
    Core/Mesh/PolyMeshT.hh
    Core/Mesh/PolyMeshT_impl.hh
    Core/Mesh/SmartHandles.hh
    Core/Mesh/SmartRange.hh
    Core/Mesh/Status.hh
    Core/Mesh/Tags.hh
    Core/Mesh/Traits.hh
    Core/Mesh/TriConnectivity.hh
    Core/Mesh/TriMesh_ArrayKernelT.hh
    Core/Mesh/TriMeshT.hh
    Core/Mesh/TriMeshT_impl.hh
    Core/System/compiler.hh
    Core/System/config.h
    Core/System/config.hh
    Core/System/mostream.hh
    Core/System/omstream.hh
    Core/System/OpenMeshDLLMacros.hh
    Core/Utils/AutoPropertyHandleT.hh
    Core/Utils/BaseProperty.hh
    Core/Utils/color_cast.hh
    Core/Utils/Endian.hh
    Core/Utils/GenProg.hh
    Core/Utils/HandleToPropHandle.hh
    Core/Utils/Noncopyable.hh
    Core/Utils/Predicates.hh
    Core/Utils/PropertyContainer.hh
    Core/Utils/PropertyCreator.hh
    Core/Utils/Property.hh
    Core/Utils/PropertyManager.hh
    Core/Utils/RandomNumberGenerator.hh
    Core/Utils/SingletonT.hh
    Core/Utils/SingletonT_impl.hh
    Core/Utils/typename.hh
    Core/Utils/vector_cast.hh
    Core/Utils/vector_traits.hh
    Tools/Decimater/BaseDecimaterT.hh
    Tools/Decimater/BaseDecimaterT_impl.hh
    Tools/Decimater/CollapseInfoT.hh
    Tools/Decimater/DecimaterT.hh
    Tools/Decimater/DecimaterT_impl.hh
    Tools/Decimater/McDecimaterT.hh
    Tools/Decimater/McDecimaterT_impl.hh
    Tools/Decimater/MixedDecimaterT.hh
    Tools/Decimater/MixedDecimaterT_impl.hh
    Tools/Decimater/ModAspectRatioT.hh
    Tools/Decimater/ModAspectRatioT_impl.hh
    Tools/Decimater/ModBaseT.hh
    Tools/Decimater/ModEdgeLengthT.hh
    Tools/Decimater/ModEdgeLengthT_impl.hh
    Tools/Decimater/ModHausdorffT.hh
    Tools/Decimater/ModHausdorffT_impl.hh
    Tools/Decimater/ModIndependentSetsT.hh
    Tools/Decimater/ModNormalDeviationT.hh
    Tools/Decimater/ModNormalFlippingT.hh
    Tools/Decimater/ModProgMeshT.hh
    Tools/Decimater/ModProgMeshT_impl.hh
    Tools/Decimater/ModQuadricT.hh
    Tools/Decimater/ModQuadricT_impl.hh
    Tools/Decimater/ModRoundnessT.hh
    Tools/Decimater/Observer.hh
    Tools/Dualizer/meshDualT.hh
    Tools/Kernel_OSG/ArrayKernelT.hh
    Tools/Kernel_OSG/AttribKernelT.hh
    Tools/Kernel_OSG/bindT.hh
    Tools/Kernel_OSG/color_cast.hh
    Tools/Kernel_OSG/PropertyKernel.hh
    Tools/Kernel_OSG/PropertyT.hh
    Tools/Kernel_OSG/Traits.hh
    Tools/Kernel_OSG/TriMesh_OSGArrayKernelT.hh
    Tools/Kernel_OSG/VectorAdapter.hh
    Tools/Smoother/JacobiLaplaceSmootherT.hh
    Tools/Smoother/JacobiLaplaceSmootherT_impl.hh
    Tools/Smoother/LaplaceSmootherT.hh
    Tools/Smoother/LaplaceSmootherT_impl.hh
    Tools/Smoother/SmootherT.hh
    Tools/Smoother/SmootherT_impl.hh
    Tools/Smoother/smooth_mesh.hh
    Tools/Subdivider/Adaptive/Composite/CompositeT.hh
    Tools/Subdivider/Adaptive/Composite/CompositeT_impl.hh
    Tools/Subdivider/Adaptive/Composite/CompositeTraits.hh
    Tools/Subdivider/Adaptive/Composite/RuleInterfaceT.hh
    Tools/Subdivider/Adaptive/Composite/RulesT.hh
    Tools/Subdivider/Adaptive/Composite/RulesT_impl.hh
    Tools/Subdivider/Adaptive/Composite/Traits.hh
    Tools/Subdivider/Uniform/CatmullClarkT.hh
    Tools/Subdivider/Uniform/CatmullClarkT_impl.hh
    Tools/Subdivider/Uniform/Composite/CompositeT.hh
    Tools/Subdivider/Uniform/Composite/CompositeT_impl.hh
    Tools/Subdivider/Uniform/Composite/CompositeTraits.hh
    Tools/Subdivider/Uniform/CompositeLoopT.hh
    Tools/Subdivider/Uniform/CompositeSqrt3T.hh
    Tools/Subdivider/Uniform/LongestEdgeT.hh
    Tools/Subdivider/Uniform/LoopT.hh
    Tools/Subdivider/Uniform/MidpointT.hh
    Tools/Subdivider/Uniform/ModifiedButterFlyT.hh
    Tools/Subdivider/Uniform/Sqrt3InterpolatingSubdividerLabsikGreinerT.hh
    Tools/Subdivider/Uniform/Sqrt3T.hh
    Tools/Subdivider/Uniform/SubdividerT.hh
    Tools/Utils/Config.hh
    Tools/Utils/conio.hh
    Tools/Utils/getopt.h
    Tools/Utils/GLConstAsString.hh
    Tools/Utils/Gnuplot.hh
    Tools/Utils/HeapT.hh
    Tools/Utils/MeshCheckerT.hh
    Tools/Utils/MeshCheckerT_impl.hh
    Tools/Utils/NumLimitsT.hh
    Tools/Utils/StripifierT.hh
    Tools/Utils/StripifierT_impl.hh
    Tools/Utils/TestingFramework.hh
    Tools/Utils/Timer.hh
    Tools/VDPM/MeshTraits.hh
    Tools/VDPM/StreamingDef.hh
    Tools/VDPM/VFront.hh
    Tools/VDPM/VHierarchy.hh
    Tools/VDPM/VHierarchyNode.hh
    Tools/VDPM/VHierarchyNodeIndex.hh
    Tools/VDPM/VHierarchyWindow.hh
    Tools/VDPM/ViewingParameters.hh
  )

  ExternalProject_ByProducts(openmesh OPENMESH_BLD ${OPENMESH_INSTDIR} ${INCLUDE_DIR}/OpenMesh
    ${OPENMESH_HDRS}
    )
  set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} OpenMesh CACHE STRING "Bundled system include dirs" FORCE)

  # OpenMesh needs slightly different target management, so we
  # we need to make sure the dependencies are established for
  # the targets ExternalProject_ByProducts isn't aware of
  foreach(OMT ${OPENMESH_LIBS})
    if (TARGET ${OMT} AND TARGET openmesh_stage)
      add_dependencies(${OMT} openmesh_stage)
    endif (TARGET ${OMT} AND TARGET openmesh_stage)
  endforeach(OMT ${OPENMESH_LIBS})

  set(OPENMESH_CORE_GEOMETRY_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/OpenMesh/Core/Geometry CACHE STRING "Building bundled OpenMesh" FORCE)
  set(OPENMESH_CORE_IO_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/OpenMesh/Core/IO CACHE STRING "Building bundled OpenMesh" FORCE)
  set(OPENMESH_CORE_MESH_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/OpenMesh/Core/Mesh CACHE STRING "Building bundled OpenMesh" FORCE)
  set(OPENMESH_CORE_SYSTEM_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/OpenMesh/Core/System CACHE STRING "Building bundled OpenMesh" FORCE)
  set(OPENMESH_CORE_UTILS_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/OpenMesh/Core/Utils CACHE STRING "Building bundled OpenMesh" FORCE)
  set(OPENMESH_TOOLS_DECIMATER_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/OpenMesh/Tools/Decimater CACHE STRING "Building bundled OpenMesh" FORCE)
  set(OPENMESH_TOOLS_DUALIZER_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/OpenMesh/Tools/Dualizer CACHE STRING "Building bundled OpenMesh" FORCE)
  set(OPENMESH_TOOLS_KERNERL_OSG_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/OpenMesh/Tools/Kernel_OSG CACHE STRING "Building bundled OpenMesh" FORCE)
  set(OPENMESH_TOOLS_SMOOTHER_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/OpenMesh/Tools/Smoother CACHE STRING "Building bundled OpenMesh" FORCE)
  set(OPENMESH_TOOLS_SUBDIVIDER_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/OpenMesh/Tools/Subdivider CACHE STRING "Building bundled OpenMesh" FORCE)
  set(OPENMESH_TOOLS_UTILS_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/OpenMesh/Tools/Utils CACHE STRING "Building bundled OpenMesh" FORCE)
  set(OPENMESH_TOOLS_VDPM_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/OpenMesh/Tools/VDPM CACHE STRING "Building bundled OpenMesh" FORCE)
  
  set(OPENMESH_INCLUDE_DIRS
    ${OPENMESH_CORE_GEOMETRY_DIR}
    ${OPENMESH_CORE_IO_DIR}
    ${OPENMESH_CORE_MESH_DIR}
    ${OPENMESH_CORE_SYSTEM_DIR}
    ${OPENMESH_CORE_UTILS_DIR}
    ${OPENMESH_TOOLS_DECIMATER_DIR}
    ${OPENMESH_TOOLS_DUALIZER_DIR}
    ${OPENMESH_TOOLS_KERNERL_OSG_DIR}
    ${OPENMESH_TOOLS_SMOOTHER_DIR}
    ${OPENMESH_TOOLS_SUBDIVIDER_DIR}
    ${OPENMESH_TOOLS_UTILS_DIR}
    ${OPENMESH_TOOLS_VDPM_DIR}
    CACHE STRING "Directories containing OPENMESH headers." FORCE)

  set(OPENMESH_LIBRARIES
    ${OPENMESH_LIBS}
    CACHE STRING "OPENMESH libraries." FORCE)

  SetTargetFolder(OPENMESH_BLD "Third Party Libraries")
  SetTargetFolder(openmesh "Third Party Libraries")

  # OpenMesh generates windows dll's in the root directory unlike most other builds which
  # stuff them in bin/
  # copy them over so our ext build logic can find them
  if(MSVC)
    foreach(OMLIB ${OPENMESH_LIBS})
    add_custom_command(TARGET OPENMESH_BLD POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${OPENMESH_INSTDIR}/${OPENMESH_BASENAME}${OMLIB}${OPENMESH_SUFFIX}
      ${OPENMESH_INSTDIR}/bin/${OPENMESH_BASENAME}${OMLIB}${OPENMESH_SUFFIX})
    endforeach(OMLIB ${OPENMESH_LIBS})
  endif(MSVC)

else (BRLCAD_OPENMESH_BUILD)

  #set(OPENMESH_LIBRARIES ${OPENMESH_LIBRARY} CACHE STRING "openmesh" FORCE)
  #set(OPENMESH_INCLUDE_DIRS "${OPENMESH_INCLUDE_DIR}" CACHE STRING "Directories containing OPENMESH headers." FORCE)

endif (BRLCAD_OPENMESH_BUILD)

mark_as_advanced(OPENMESH_INCLUDE_DIRS)
mark_as_advanced(OPENMESH_LIBRARIES)
mark_as_advanced(OPENMESH_CORE_GEOMETRY_DIR)
mark_as_advanced(OPENMESH_CORE_IO_DIR)
mark_as_advanced(OPENMESH_CORE_MESH_DIR)
mark_as_advanced(OPENMESH_CORE_SYSTEM_DIR)
mark_as_advanced(OPENMESH_CORE_UTILS_DIR)
mark_as_advanced(OPENMESH_TOOLS_DECIMATER_DIR)
mark_as_advanced(OPENMESH_TOOLS_DUALIZER_DIR)
mark_as_advanced(OPENMESH_TOOLS_KERNERL_OSG_DIR)
mark_as_advanced(OPENMESH_TOOLS_SMOOTHER_DIR)
mark_as_advanced(OPENMESH_TOOLS_SUBDIVIDER_DIR)
mark_as_advanced(OPENMESH_TOOLS_UTILS_DIR)
mark_as_advanced(OPENMESH_TOOLS_VDPM_DIR)

include("${CMAKE_CURRENT_SOURCE_DIR}/openmesh.dist")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

