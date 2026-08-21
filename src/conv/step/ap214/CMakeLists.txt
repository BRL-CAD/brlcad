set(STEP_SCHEMA_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ap214e3.exp)
set(AP214_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  generate_schema_inputs(${STEP_SCHEMA_FILE} ap214g ap214)
  generate_schema_inputs(${STEP_SCHEMA_FILE} gap214 ap214)

set(
  gap214_SOURCES
  ${gap214_express_srcs}
  ../BRLCADWrapper.cpp
  ../STEPMechanicalExport.cpp
  ../STEPConfigurationExport.cpp
  ../STEPConfigurationIdentityExport.cpp
  ../STEPExportMetadata.cpp
  ../STEPMaterialExport.cpp
  ../STEPString.cpp
  ../g-step/AP203.cpp
  ../g-step/Assembly_Product.cpp
  ../g-step/Comb.cpp
  ../g-step/Default_Geometric_Context.cpp
  ../STEPNativeCSGExport.cpp
  ../g-step/ON_Brep.cpp
  ../g-step/ON_NurbsCurve.cpp
  ../g-step/ON_NurbsSurface.cpp
  ../g-step/Shape_Representation.cpp
  ../g-step/Shape_Representation_Relationship.cpp
  ../g-step/G_Objects.cpp
  ../g-step/Trees.cpp
  ../STEPProductExport.cpp
  Export.cpp
)

set(
  ap214g_SOURCES
  ../BRLCADWrapper.cpp
  ../STEPString.cpp
  ../STEPBudget.cpp
  ../STEPBrepValidation.cpp
  ../STEPWrapper.cpp
  ../STEPWrapperAttributes.cpp
  ../STEPWrapperIO.cpp
  AP214Adapter.cpp
  ../STEPNativeCSGImport.cpp
  ../STEPMaterialImport.cpp
  AP214Presentation.cpp
  Import.cpp
  ../step-g/AdvancedBrepShapeRepresentation.cpp
  ../step-g/AdvancedFace.cpp
  ../step-g/AmountOfSubstanceContextDependentUnit.cpp
  ../step-g/AmountOfSubstanceConversionBasedUnit.cpp
  ../step-g/AmountOfSubstanceSiUnit.cpp
  ../step-g/AmountOfSubstanceUnit.cpp
  ../step-g/ApplicationContext.cpp
  ../step-g/ApplicationContextElement.cpp
  ../step-g/AssemblyComponentUsage.cpp
  ../step-g/AreaContextDependentUnit.cpp
  ../step-g/AreaConversionBasedUnit.cpp
  ../step-g/AreaSiUnit.cpp
  ../step-g/AreaUnit.cpp
  ../step-g/Axis1Placement.cpp
  ../step-g/Axis2Placement.cpp
  ../step-g/Axis2Placement2D.cpp
  ../step-g/Axis2Placement3D.cpp
  ../step-g/BSplineCurve.cpp
  ../step-g/BSplineCurveWithKnots.cpp
  ../step-g/BSplineSurface.cpp
  ../step-g/BSplineSurfaceWithKnots.cpp
  ../step-g/BezierCurve.cpp
  ../step-g/BezierSurface.cpp
  ../step-g/BoundaryCurve.cpp
  ../step-g/BoundedCurve.cpp
  ../step-g/BoundedPCurve.cpp
  ../step-g/BoundedSurface.cpp
  ../step-g/BoundedSurfaceCurve.cpp
  ../step-g/BrepWithVoids.cpp
  ../step-g/CartesianPoint.cpp
  ../step-g/CartesianTransformationOperator.cpp
  ../step-g/CartesianTransformationOperator2D.cpp
  ../step-g/CartesianTransformationOperator3D.cpp
  ../step-g/CharacterizedDefinition.cpp
  ../step-g/CharacterizedProductDefinition.cpp
  ../step-g/Circle.cpp
  ../step-g/ClosedShell.cpp
  ../step-g/CompositeCurve.cpp
  ../step-g/CompositeCurveOnSurface.cpp
  ../step-g/CompositeCurveSegment.cpp
  ../step-g/CompoundRepresentationItem.cpp
  ../step-g/Conic.cpp
  ../step-g/ConicalSurface.cpp
  ../step-g/ConnectedFaceSet.cpp
  ../step-g/ContextDependentShapeRepresentation.cpp
  ../step-g/ContextDependentUnit.cpp
  ../step-g/ConversionBasedUnit.cpp
  ../step-g/Curve.cpp
  ../step-g/CurveBoundedSurface.cpp
  ../step-g/CurveReplica.cpp
  ../step-g/CsgShapeRepresentation.cpp
  ../step-g/CsgSolid.cpp
  ../step-g/CylindricalSurface.cpp
  ../step-g/DefinitionalRepresentation.cpp
  ../step-g/DerivedUnit.cpp
  ../step-g/DerivedUnitElement.cpp
  ../step-g/DesignContext.cpp
  ../step-g/DimensionalExponents.cpp
  ../step-g/Direction.cpp
  ../step-g/Edge.cpp
  ../step-g/EdgeCurve.cpp
  ../step-g/EdgeLoop.cpp
  ../step-g/ElectricCurrentContextDependentUnit.cpp
  ../step-g/ElectricCurrentConversionBasedUnit.cpp
  ../step-g/ElectricCurrentSiUnit.cpp
  ../step-g/ElectricCurrentUnit.cpp
  ../step-g/ElementarySurface.cpp
  ../step-g/Ellipse.cpp
  ../step-g/ExtrudedAreaSolid.cpp
  ../step-g/Face.cpp
  ../step-g/FaceBound.cpp
  ../step-g/FaceOuterBound.cpp
  ../step-g/FaceSurface.cpp
  ../step-g/FacetedBrep.cpp
  ../step-g/FacetedBrepShapeRepresentation.cpp
  ../step-g/PolyLoop.cpp
  ../step-g/Factory.cpp
  ../step-g/FoundedItem.cpp
  ../step-g/FunctionallyDefinedTransformation.cpp
  ../step-g/GeometricRepresentationContext.cpp
  ../step-g/GeometricRepresentationItem.cpp
  ../step-g/GeometricSet.cpp
  ../step-g/GeometricSetSelect.cpp
  ../step-g/GeometricallyBoundedSurfaceShapeRepresentation.cpp
  ../step-g/GeometricallyBoundedWireframeShapeRepresentation.cpp
  ../step-g/GlobalUncertaintyAssignedContext.cpp
  ../step-g/GlobalUnitAssignedContext.cpp
  ../step-g/Hyperbola.cpp
  ../step-g/IntersectionCurve.cpp
  ../step-g/ItemDefinedTransformation.cpp
  ../step-g/LengthContextDependentUnit.cpp
  ../step-g/LengthConversionBasedUnit.cpp
  ../step-g/LengthMeasureWithUnit.cpp
  ../step-g/LengthSiUnit.cpp
  ../step-g/LengthUnit.cpp
  ../step-g/Line.cpp
  ../step-g/LocalUnits.cpp
  ../step-g/Loop.cpp
  ../step-g/LuminousIntensityContextDependentUnit.cpp
  ../step-g/LuminousIntensityConversionBasedUnit.cpp
  ../step-g/LuminousIntensitySiUnit.cpp
  ../step-g/LuminousIntensityUnit.cpp
  ../step-g/ManifoldSolidBrep.cpp
  ../step-g/ManifoldSurfaceShapeRepresentation.cpp
  ../step-g/MappedItem.cpp
  ../step-g/MassContextDependentUnit.cpp
  ../step-g/MassConversionBasedUnit.cpp
  ../step-g/MassSiUnit.cpp
  ../step-g/MassUnit.cpp
  ../step-g/MeasureValue.cpp
  ../step-g/MeasureWithUnit.cpp
  ../step-g/MechanicalContext.cpp
  ../step-g/NamedUnit.cpp
  ../step-g/NextAssemblyUsageOccurrence.cpp
  ../step-g/OffsetCurve2D.cpp
  ../step-g/OffsetCurve3D.cpp
  ../step-g/OffsetSurface.cpp
  ../step-g/OpenNurbsInterfaces.cpp
  ../step-g/OpenNurbsPrimitives.cpp
  ../step-g/OpenNurbsSplines.cpp
  ../step-g/OpenShell.cpp
  ../step-g/OrientedEdge.cpp
  ../step-g/OrientedClosedShell.cpp
  ../step-g/OrientedFace.cpp
  ../step-g/PCurve.cpp
  ../step-g/PCurveOrSurface.cpp
  ../step-g/Parabola.cpp
  ../step-g/ParametricRepresentationContext.cpp
  ../step-g/Path.cpp
  ../step-g/Placement.cpp
  ../step-g/Plane.cpp
  ../step-g/PlaneAngleContextDependentUnit.cpp
  ../step-g/PlaneAngleConversionBasedUnit.cpp
  ../step-g/PlaneAngleMeasureWithUnit.cpp
  ../step-g/PlaneAngleSiUnit.cpp
  ../step-g/PlaneAngleUnit.cpp
  ../step-g/Point.cpp
  ../step-g/Polyline.cpp
  ../step-g/Product.cpp
  ../step-g/ProductCategory.cpp
  ../step-g/ProductContext.cpp
  ../step-g/ProductDefinition.cpp
  ../step-g/ProductDefinitionContext.cpp
  ../step-g/ProductDefinitionContextAssociation.cpp
  ../step-g/ProductDefinitionContextRole.cpp
  ../step-g/ProductDefinitionFormation.cpp
  ../step-g/ProductDefinitionFormationWithSpecifiedSource.cpp
  ../step-g/ProductDefinitionRelationship.cpp
  ../step-g/ProductDefinitionShape.cpp
  ../step-g/ProductDefinitionUsage.cpp
  ../step-g/ProductDefinitionWithAssociatedDocuments.cpp
  ../step-g/ProductRelatedProductCategory.cpp
  ../step-g/PropertyDefinition.cpp
  ../step-g/PropertyDefinitionRepresentation.cpp
  ../step-g/QuasiUniformCurve.cpp
  ../step-g/QuasiUniformSurface.cpp
  ../step-g/RatioContextDependentUnit.cpp
  ../step-g/RatioConversionBasedUnit.cpp
  ../step-g/RatioSiUnit.cpp
  ../step-g/RatioUnit.cpp
  ../step-g/RationalBSplineCurve.cpp
  ../step-g/RationalBSplineCurveWithKnots.cpp
  ../step-g/RationalBSplineSurface.cpp
  ../step-g/RationalBSplineSurfaceWithKnots.cpp
  ../step-g/RationalBezierCurve.cpp
  ../step-g/RationalBezierSurface.cpp
  ../step-g/RationalQuasiUniformCurve.cpp
  ../step-g/RationalQuasiUniformSurface.cpp
  ../step-g/RationalUniformCurve.cpp
  ../step-g/RationalUniformSurface.cpp
  ../step-g/RectangularCompositeSurface.cpp
  ../step-g/RectangularTrimmedSurface.cpp
  ../step-g/Representation.cpp
  ../step-g/RepresentationContext.cpp
  ../step-g/RepresentationItem.cpp
  ../step-g/RepresentationMap.cpp
  ../step-g/RepresentationRelationship.cpp
  ../step-g/RepresentationRelationshipWithTransformation.cpp
  ../step-g/RepresentedDefinition.cpp
  ../step-g/RevolvedAreaSolid.cpp
  ../step-g/STEPEntity.cpp
  ../step-g/SeamCurve.cpp
  ../step-g/ShapeAspect.cpp
  ../step-g/ShapeAspectRelationship.cpp
  ../step-g/ShapeDefinition.cpp
  ../step-g/ShapeDefinitionRepresentation.cpp
  ../step-g/ShapeRepresentation.cpp
  ../step-g/ShapeRepresentationRelationship.cpp
  ../step-g/ShellBasedSurfaceModel.cpp
  ../step-g/SiUnit.cpp
  ../step-g/SolidAngleContextDependentUnit.cpp
  ../step-g/SolidAngleConversionBasedUnit.cpp
  ../step-g/SolidAngleSiUnit.cpp
  ../step-g/SolidAngleUnit.cpp
  ../step-g/SolidModel.cpp
  ../step-g/SolidReplica.cpp
  ../step-g/SphericalSurface.cpp
  ../step-g/Surface.cpp
  ../step-g/SurfaceCurve.cpp
  ../step-g/SurfaceOfLinearExtrusion.cpp
  ../step-g/SurfaceOfRevolution.cpp
  ../step-g/SurfacePatch.cpp
  ../step-g/SurfaceReplica.cpp
  ../step-g/SweptAreaSolid.cpp
  ../step-g/SweptSurface.cpp
  ../step-g/ThermodynamicTemperatureContextDependentUnit.cpp
  ../step-g/ThermodynamicTemperatureConversionBasedUnit.cpp
  ../step-g/ThermodynamicTemperatureSiUnit.cpp
  ../step-g/ThermodynamicTemperatureUnit.cpp
  ../step-g/TimeContextDependentUnit.cpp
  ../step-g/TimeConversionBasedUnit.cpp
  ../step-g/TimeSiUnit.cpp
  ../step-g/TimeUnit.cpp
  ../step-g/TopologicalRepresentationItem.cpp
  ../step-g/ToroidalSurface.cpp
  ../step-g/Transformation.cpp
  ../step-g/TrimmedCurve.cpp
  ../step-g/TrimmingSelect.cpp
  ../step-g/UncertaintyMeasureWithUnit.cpp
  ../step-g/UniformCurve.cpp
  ../step-g/UniformSurface.cpp
  ../step-g/Unit.cpp
  ../step-g/Vector.cpp
  ../step-g/Vertex.cpp
  ../step-g/VertexLoop.cpp
  ../step-g/VertexPoint.cpp
  ../step-g/VolumeContextDependentUnit.cpp
  ../step-g/VolumeConversionBasedUnit.cpp
  ../step-g/VolumeSiUnit.cpp
  ../step-g/VolumeUnit.cpp
)

set(
  ap214g_LIBS
  libwdb
  librt
  libbu
  libbrep
  ${ap214g_SCHEMA_BINDINGS_TARGET}
  ${STEPCODE_UTILS_LIBRARY}
  ${STEPCODE_EDITOR_LIBRARY}
  ${STEPCODE_DAI_LIBRARY}
  ${STEPCODE_CORE_LIBRARY}
  OPENNURBS::OPENNURBS
)

if(STEPCODE_LAZY_FOUND)
  list(APPEND ap214g_SOURCES ../STEPLazySession.cpp)
  list(APPEND ap214g_LIBS ${STEPCODE_LAZY_LIBRARY})
endif()

set(
  AP214_INCLUDE_DIRS
  ${CMAKE_CURRENT_BINARY_DIR}
  ${ap214g_SCHEMA_OUT_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../step-g
  ${STEPCODE_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src/libbrep # for PullbackCurve.h
)

if(BRLCAD_ENABLE_STEP AND BUILD_TESTING)
  step_use_import_common_objects(ap214g_TARGET_SOURCES ap214g_SOURCES)
  step_record_schema_sources(AP214 IMPORT ap214g_TARGET_SOURCES)
  step_record_schema_sources(AP214 EXPORT gap214_SOURCES)

  brlcad_addexec(
    ap214_import_test
    "tests/ap214_import_test.cpp;AP214Adapter.cpp;../STEPString.cpp"
    "libbu"
    TEST
  )
  target_include_directories(ap214_import_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)
  brlcad_add_test(NAME ap214_import_adapter COMMAND ap214_import_test)
  brlcad_addexec(
    ap214_brep_transaction_test
    "tests/ap214_brep_transaction_test.cpp;../STEPBrepValidation.cpp"
    "libbrep;libbu;OPENNURBS::OPENNURBS"
    TEST
  )
  target_include_directories(
    ap214_brep_transaction_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
  )
  brlcad_add_test(
    NAME ap214_brep_transaction_validation
    COMMAND ap214_brep_transaction_test
  )
  if(STEPCODE_LAZY_FOUND)
    brlcad_addexec(
      ap214_lazy_session_test
      "tests/ap214_lazy_session_test.cpp;../STEPLazySession.cpp;../StepSchemaRuntime.cpp"
      "${ap214g_SCHEMA_BINDINGS_TARGET};${STEPCODE_LAZY_LIBRARY};${STEPCODE_EDITOR_LIBRARY};${STEPCODE_DAI_LIBRARY};${STEPCODE_CORE_LIBRARY};${STEPCODE_UTILS_LIBRARY};libbu"
      TEST
    )
    target_include_directories(ap214_lazy_session_test BEFORE PRIVATE ${AP214_INCLUDE_DIRS})
    target_compile_definitions(ap214_lazy_session_test PRIVATE AP214e3 SC_SCHEMA_DLL_EXPORTS)
    get_filename_component(_stepcode_runtime_dir "${STEPCODE_CORE_LIBRARY}" DIRECTORY)
    set_property(TARGET ap214_lazy_session_test APPEND PROPERTY
      BUILD_RPATH "${_stepcode_runtime_dir}"
    )
    add_dependencies(ap214_lazy_session_test ${ap214g_SCHEMA_TARGET})
    brlcad_add_test(
      NAME ap214_lazy_session
      COMMAND ap214_lazy_session_test
        ${AP214_TEST_DIR}/ap214_direct_manifold.stp
        ${AP214_TEST_DIR}/ap214_lazy_diagnostics.stp
        ${AP214_TEST_DIR}/ap214_legacy_contexts.stp
    )
  endif()
  brlcad_add_test(
    NAME ap214_style_metadata
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_style_layer.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_style_layer.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_style_layer.g
      -P ${AP214_TEST_DIR}/ap214_style_report.cmake
  )
  brlcad_add_test(
    NAME ap214_dry_run
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_direct_manifold.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_dry_run.json
      -DSUMMARY=${CMAKE_CURRENT_BINARY_DIR}/ap214_dry_run.csv
      -P ${AP214_TEST_DIR}/ap214_dry_run.cmake
  )
  brlcad_add_test(
    NAME ap214_budget_controls
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_direct_manifold.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_budget_controls.json
      -P ${AP214_TEST_DIR}/ap214_budget_controls.cmake
  )
  brlcad_add_test(
    NAME ap214_lazy_diagnostics
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_lazy_diagnostics.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_lazy_diagnostics.json
      -P ${AP214_TEST_DIR}/ap214_lazy_diagnostics.cmake
  )
  brlcad_add_test(
    NAME ap214_lazy_many_solids
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_lazy_many_solids.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_lazy_many_solids.json
      -P ${AP214_TEST_DIR}/ap214_lazy_many_solids.cmake
  )
  brlcad_add_test(
    NAME ap214_entity_selection
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_lazy_many_solids.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_entity_selection.json
      -P ${AP214_TEST_DIR}/ap214_entity_selection.cmake
  )
  brlcad_add_test(
    NAME ap214_deterministic_output
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_direct_manifold.stp
      -DOUTPUT_A=${CMAKE_CURRENT_BINARY_DIR}/ap214_deterministic_a.g
      -DOUTPUT_B=${CMAKE_CURRENT_BINARY_DIR}/ap214_deterministic_b.g
      -P ${AP214_TEST_DIR}/ap214_deterministic_output.cmake
  )
  brlcad_add_test(
    NAME ap214_parallel_determinism
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_brep_with_voids.stp
      -DOUTPUT_SERIAL=${CMAKE_CURRENT_BINARY_DIR}/ap214_parallel_serial.g
      -DOUTPUT_PARALLEL=${CMAKE_CURRENT_BINARY_DIR}/ap214_parallel_eight.g
      -DREPORT_SERIAL=${CMAKE_CURRENT_BINARY_DIR}/ap214_parallel_serial.json
      -DREPORT_PARALLEL=${CMAKE_CURRENT_BINARY_DIR}/ap214_parallel_eight.json
      -DEXPECTED_GEOMETRY=2
      -P ${AP214_TEST_DIR}/ap214_parallel_determinism.cmake
  )
  brlcad_add_test(
    NAME ap214_direct_parallel_determinism
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_direct_manifold.stp
      -DOUTPUT_SERIAL=${CMAKE_CURRENT_BINARY_DIR}/ap214_direct_parallel_serial.g
      -DOUTPUT_PARALLEL=${CMAKE_CURRENT_BINARY_DIR}/ap214_direct_parallel_eight.g
      -DREPORT_SERIAL=${CMAKE_CURRENT_BINARY_DIR}/ap214_direct_parallel_serial.json
      -DREPORT_PARALLEL=${CMAKE_CURRENT_BINARY_DIR}/ap214_direct_parallel_eight.json
      -DEXPECTED_GEOMETRY=1
      -P ${AP214_TEST_DIR}/ap214_parallel_determinism.cmake
  )
  brlcad_add_test(
    NAME ap214_missing_face_reference
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_direct_manifold.stp
      -DMALFORMED_INPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_missing_face_reference.stp
      -DOUTPUT_SAFE=${CMAKE_CURRENT_BINARY_DIR}/ap214_missing_face_reference_safe.g
      -DOUTPUT_NONE=${CMAKE_CURRENT_BINARY_DIR}/ap214_missing_face_reference_none.g
      -DREPORT_SAFE=${CMAKE_CURRENT_BINARY_DIR}/ap214_missing_face_reference_safe.json
      -DREPORT_NONE=${CMAKE_CURRENT_BINARY_DIR}/ap214_missing_face_reference_none.json
      -P ${AP214_TEST_DIR}/ap214_missing_face_reference.cmake
  )
  brlcad_add_test(
    NAME ap214_parallel_failure
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_parallel_failure.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_parallel_failure.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_parallel_failure.g
      -P ${AP214_TEST_DIR}/ap214_parallel_failure.cmake
  )
  brlcad_add_test(
    NAME ap214_direct_manifold
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_direct_manifold.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_direct_manifold.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_direct_manifold.g
      -P ${AP214_TEST_DIR}/ap214_direct_manifold.cmake
  )
  brlcad_add_test(
    NAME ap214_missing_planar_triangular_cap
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_direct_manifold.stp
      -DMALFORMED_INPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_missing_planar_triangular_cap.stp
      -DMULTI_FACE_INPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_multiple_missing_faces.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_missing_planar_triangular_cap.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_missing_planar_triangular_cap.g
      -P ${AP214_TEST_DIR}/ap214_missing_planar_triangular_cap.cmake
  )
  brlcad_add_test(
    NAME ap214_repeated_vertex_periodic_lobe
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_repeated_vertex_periodic_lobe.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_repeated_vertex_periodic_lobe.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_repeated_vertex_periodic_lobe.g
      -P ${AP214_TEST_DIR}/ap214_repeated_vertex_periodic_lobe.cmake
  )
  brlcad_add_test(
    NAME ap214_surface_shells
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_surface_shells.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_surface_shells.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_surface_shells.g
      -P ${AP214_TEST_DIR}/ap214_surface_shells.cmake
  )
  brlcad_add_test(
    NAME ap214_keyhole_loop
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_keyhole_loop.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_keyhole_loop.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_keyhole_loop.g
      -P ${AP214_TEST_DIR}/ap214_keyhole_loop.cmake
  )
  brlcad_add_test(
    NAME ap214_periodic_torus
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_periodic_torus.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_periodic_torus.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_periodic_torus.g
      -P ${AP214_TEST_DIR}/ap214_periodic_torus.cmake
  )
  brlcad_add_test(
    NAME ap214_vertex_loop_torus
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_vertex_loop_torus.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_vertex_loop_torus.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_vertex_loop_torus.g
      -P ${AP214_TEST_DIR}/ap214_vertex_loop_torus.cmake
  )
  brlcad_add_test(
    NAME ap214_vertex_loop_sphere
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_vertex_loop_sphere.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_vertex_loop_sphere.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_vertex_loop_sphere.g
      -P ${AP214_TEST_DIR}/ap214_vertex_loop_sphere.cmake
  )
  brlcad_add_test(
    NAME ap214_vertex_loop_conical_face
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_vertex_loop_conical_face.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_vertex_loop_conical_face.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_vertex_loop_conical_face.g
      -P ${AP214_TEST_DIR}/ap214_vertex_loop_conical_face.cmake
  )
  brlcad_add_test(
    NAME ap214_vertex_loop_spline_pole
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_vertex_loop_spline_pole.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_vertex_loop_spline_pole.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_vertex_loop_spline_pole.g
      -P ${AP214_TEST_DIR}/ap214_vertex_loop_spline_pole.cmake
  )
  brlcad_add_test(
    NAME ap214_sub_tolerance_torus_seam
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_sub_tolerance_torus_seam.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_sub_tolerance_torus_seam.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_sub_tolerance_torus_seam.g
      -P ${AP214_TEST_DIR}/ap214_sub_tolerance_torus_seam.cmake
  )
  brlcad_add_test(
    NAME ap214_negative_spindle_torus
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_negative_spindle_torus.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_negative_spindle_torus.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_negative_spindle_torus.g
      -P ${AP214_TEST_DIR}/ap214_negative_spindle_torus.cmake
  )
  brlcad_add_test(
    NAME ap214_collapsed_torus_seam
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_collapsed_torus_seam.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_collapsed_torus_seam.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_collapsed_torus_seam.g
      -P ${AP214_TEST_DIR}/ap214_collapsed_torus_seam.cmake
  )
  brlcad_add_test(
    NAME ap214_degenerate_torus_endpoint
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_degenerate_torus_endpoint.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_degenerate_torus_endpoint.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_degenerate_torus_endpoint.g
      -P ${AP214_TEST_DIR}/ap214_degenerate_torus_endpoint.cmake
  )
  brlcad_add_test(
    NAME ap214_horn_torus_keyhole
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_horn_torus_keyhole.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_horn_torus_keyhole.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_horn_torus_keyhole.g
      -P ${AP214_TEST_DIR}/ap214_horn_torus_keyhole.cmake
  )
  brlcad_add_test(
    NAME ap214_degenerate_torus_sheet
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_degenerate_torus_sheet.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_degenerate_torus_sheet.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_degenerate_torus_sheet.g
      -P ${AP214_TEST_DIR}/ap214_degenerate_torus_sheet.cmake
  )
  brlcad_add_test(
    NAME ap214_short_cylinder_seam
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_short_cylinder_seam.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_short_cylinder_seam.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_short_cylinder_seam.g
      -P ${AP214_TEST_DIR}/ap214_short_cylinder_seam.cmake
  )
  brlcad_add_test(
    NAME ap214_single_winding_cylinder_boundary_invalid
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_single_winding_cylinder_boundary_invalid.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_single_winding_cylinder_boundary_invalid.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_single_winding_cylinder_boundary_invalid.g
      -P ${AP214_TEST_DIR}/ap214_single_winding_cylinder_boundary_invalid.cmake
  )
  brlcad_add_test(
    NAME ap214_distinct_edge_keyhole_rejoin
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_distinct_edge_keyhole_rejoin.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_distinct_edge_keyhole_rejoin.json
      -DEXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_distinct_edge_keyhole_rejoin_exact.json
      -P ${AP214_TEST_DIR}/ap214_distinct_edge_keyhole_rejoin.cmake
  )
  brlcad_add_test(
    NAME ap214_face_wr2_off_surface_torus_invalid
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_face_wr2_off_surface_torus_invalid.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_face_wr2_off_surface_torus_invalid.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_face_wr2_off_surface_torus_invalid.g
      -P ${AP214_TEST_DIR}/ap214_face_wr2_off_surface_torus_invalid.cmake
  )
  brlcad_add_test(
    NAME ap214_duplicate_periodic_seam
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_duplicate_periodic_seam.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_duplicate_periodic_seam.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_duplicate_periodic_seam.g
      -P ${AP214_TEST_DIR}/ap214_duplicate_periodic_seam.cmake
  )
  brlcad_add_test(
    NAME ap214_notched_periodic_band
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_notched_periodic_band.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_notched_periodic_band.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_notched_periodic_band.g
      -P ${AP214_TEST_DIR}/ap214_notched_periodic_band.cmake
  )
  brlcad_add_test(
    NAME ap214_conical_closed_edge_band
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_conical_closed_edge_band.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_conical_closed_edge_band.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_conical_closed_edge_band.g
      -P ${AP214_TEST_DIR}/ap214_conical_closed_edge_band.cmake
  )
  brlcad_add_test(
    NAME ap214_shared_conical_surface_bands
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_shared_conical_surface_bands.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_shared_conical_surface_bands.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_shared_conical_surface_bands.g
      -P ${AP214_TEST_DIR}/ap214_shared_conical_surface_bands.cmake
  )
  brlcad_add_test(
    NAME ap214_multi_edge_conical_cap
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_multi_edge_conical_cap.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_multi_edge_conical_cap.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_multi_edge_conical_cap.g
      -P ${AP214_TEST_DIR}/ap214_multi_edge_conical_cap.cmake
  )
  brlcad_add_test(
    NAME ap214_measured_full_period_chain
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_measured_full_period_chain.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_measured_full_period_chain.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_measured_full_period_chain.g
      -P ${AP214_TEST_DIR}/ap214_measured_full_period_chain.cmake
  )
  brlcad_add_test(
    NAME ap214_four_face_keyhole
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_four_face_keyhole.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_four_face_keyhole.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_four_face_keyhole.g
      -P ${AP214_TEST_DIR}/ap214_four_face_keyhole.cmake
  )
  brlcad_add_test(
    NAME ap214_paired_singleton_keyhole
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_paired_singleton_keyhole.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_paired_singleton_keyhole.json
      "-DEXPECTED_REPAIR=aligned a periodic NURBS surface seam with its closed edge"
      -P ${AP214_TEST_DIR}/ap214_structural_repair_fixture.cmake
  )
  brlcad_add_test(
    NAME ap214_neighbor_keyhole_seam
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_neighbor_keyhole_seam.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_neighbor_keyhole_seam.json
      "-DEXPECTED_REPAIR=normalized a restored full-period seam pair within model tolerance"
      -P ${AP214_TEST_DIR}/ap214_structural_repair_fixture.cmake
  )
  # This is the smallest focused representative of a repeated two-use,
  # full-period singleton-keyhole topology.
  brlcad_add_test(
    NAME ap214_singleton_keyhole_solid
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_singleton_keyhole_solid.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_singleton_keyhole_solid.json
      -DENTITY_IDS=3505
      -DEXPECTED_ATTEMPTED=1
      -P ${AP214_TEST_DIR}/ap214_selected_solid_regression.cmake
  )
  brlcad_add_test(
    NAME ap214_spherical_antipodal_boundary
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_spherical_antipodal_boundary.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_spherical_antipodal_boundary.json
      "-DEXPECTED_REPAIR=split an exact STEP edge at a surface parameter singularity"
      -P ${AP214_TEST_DIR}/ap214_structural_repair_fixture.cmake
  )
  brlcad_add_test(
    NAME ap214_spherical_antipodal_boundary_with_hole
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_spherical_antipodal_boundary_with_hole.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_spherical_antipodal_boundary_with_hole.json
      "-DEXPECTED_REPAIR=resolved a contradictory analytic proxy tangent using directed exact-edge chords"
      -P ${AP214_TEST_DIR}/ap214_structural_repair_fixture.cmake
  )
  brlcad_add_test(
    NAME ap214_periodic_face_branch
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_periodic_face_branch.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_periodic_face_branch.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_periodic_face_branch.g
      -P ${AP214_TEST_DIR}/ap214_periodic_face_branch.cmake
  )
  brlcad_add_test(
    NAME ap214_doubly_periodic_chain
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_doubly_periodic_chain.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_doubly_periodic_chain.json
      -DEXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_doubly_periodic_chain_exact.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_doubly_periodic_chain.g
      -P ${AP214_TEST_DIR}/ap214_doubly_periodic_chain.cmake
  )
  brlcad_add_test(
    NAME ap214_shared_periodic_edge
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_shared_periodic_edge.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_shared_periodic_edge.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_shared_periodic_edge.g
      -P ${AP214_TEST_DIR}/ap214_shared_periodic_edge.cmake
  )
  brlcad_add_test(
    NAME ap214_shared_revolution_edge
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_shared_revolution_edge.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_shared_revolution_edge.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_shared_revolution_edge.g
      -P ${AP214_TEST_DIR}/ap214_shared_revolution_edge.cmake
  )
  brlcad_add_test(
    NAME ap214_contractible_periodic_neighbor
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_contractible_periodic_neighbor.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_contractible_periodic_neighbor.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_contractible_periodic_neighbor.g
      -P ${AP214_TEST_DIR}/ap214_contractible_periodic_neighbor.cmake
  )
  brlcad_add_test(
    NAME ap214_periodic_band_with_hole
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_periodic_band_with_hole.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_periodic_band_with_hole.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_periodic_band_with_hole.g
      -P ${AP214_TEST_DIR}/ap214_periodic_band_with_hole.cmake
  )
  brlcad_add_test(
    NAME ap214_measured_native_periodic_boundary
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_measured_native_periodic_boundary.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_measured_native_periodic_boundary.json
      -DEXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_measured_native_periodic_boundary_exact.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_measured_native_periodic_boundary.g
      -P ${AP214_TEST_DIR}/ap214_measured_native_periodic_boundary.cmake
  )
  brlcad_add_test(
    NAME ap214_measured_periodic_winding
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_measured_periodic_winding.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_measured_periodic_winding.json
      -DEXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_measured_periodic_winding_exact.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_measured_periodic_winding.g
      -P ${AP214_TEST_DIR}/ap214_measured_periodic_winding.cmake
  )
  brlcad_add_test(
    NAME ap214_nonmanifold_four_use_edge
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_nonmanifold_four_use_edge.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_nonmanifold_four_use_edge.json
      -P ${AP214_TEST_DIR}/ap214_nonmanifold_four_use_edge.cmake
  )
  brlcad_add_test(
    NAME ap214_periodic_endpoint_branch
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_periodic_endpoint_branch.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_periodic_endpoint_branch.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_periodic_endpoint_branch.g
      -P ${AP214_TEST_DIR}/ap214_periodic_endpoint_branch.cmake
  )
  brlcad_add_test(
    NAME ap214_segmented_cylinder_band
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_segmented_cylinder_band.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_segmented_cylinder_band.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_segmented_cylinder_band.g
      -P ${AP214_TEST_DIR}/ap214_segmented_cylinder_band.cmake
  )
  brlcad_add_test(
    NAME ap214_wavy_periodic_band
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_wavy_periodic_band.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_wavy_periodic_band.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_wavy_periodic_band.g
      -P ${AP214_TEST_DIR}/ap214_wavy_periodic_band.cmake
  )
  brlcad_add_test(
    NAME ap214_nurbs_native_seam
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_nurbs_native_seam.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_nurbs_native_seam.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_nurbs_native_seam.g
      -P ${AP214_TEST_DIR}/ap214_nurbs_native_seam.cmake
  )
  brlcad_add_test(
    NAME ap214_collapsed_wavy_periodic_band
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_collapsed_wavy_periodic_band.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_collapsed_wavy_periodic_band.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_collapsed_wavy_periodic_band.g
      -P ${AP214_TEST_DIR}/ap214_collapsed_wavy_periodic_band.cmake
  )
  brlcad_add_test(
    NAME ap214_offset_vertex_periodic_band
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_offset_vertex_periodic_band.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_offset_vertex_periodic_band.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_offset_vertex_periodic_band.g
      -P ${AP214_TEST_DIR}/ap214_offset_vertex_periodic_band.cmake
  )
  brlcad_add_test(
    NAME ap214_periodic_edge_propagation
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_periodic_edge_propagation.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_periodic_edge_propagation.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_periodic_edge_propagation.g
      -P ${AP214_TEST_DIR}/ap214_periodic_edge_propagation.cmake
  )
  brlcad_add_test(
    NAME ap214_measured_vertex_tolerance_band
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_measured_vertex_tolerance_band.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_measured_vertex_tolerance_band.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_measured_vertex_tolerance_band.g
      -DEXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_measured_vertex_tolerance_band_exact.json
      -DEXACT_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_measured_vertex_tolerance_band_exact.g
      -P ${AP214_TEST_DIR}/ap214_measured_vertex_tolerance_band.cmake
  )
  brlcad_add_test(
    NAME ap214_open_spline_vertex_tolerance
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_open_spline_vertex_tolerance.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_open_spline_vertex_tolerance.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_open_spline_vertex_tolerance.g
      -DEXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_open_spline_vertex_tolerance_exact.json
      -DEXACT_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_open_spline_vertex_tolerance_exact.g
      -P ${AP214_TEST_DIR}/ap214_open_spline_vertex_tolerance.cmake
  )
  brlcad_add_test(
    NAME ap214_permissive_inferred_curve
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_permissive_inferred_curve.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_permissive_inferred_curve.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_permissive_inferred_curve.g
      -P ${AP214_TEST_DIR}/ap214_permissive_inferred_curve.cmake
  )
  brlcad_add_test(
    NAME ap214_offset_extrusion
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_offset_extrusion.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_offset_extrusion.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_offset_extrusion.g
      -P ${AP214_TEST_DIR}/ap214_offset_extrusion.cmake
  )
  brlcad_add_test(
    NAME ap214_offset_revolution
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_offset_revolution.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_offset_revolution.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_offset_revolution.g
      -P ${AP214_TEST_DIR}/ap214_offset_revolution.cmake
  )
  brlcad_add_test(
    NAME ap214_permissive_shared_dangling_spur
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_permissive_shared_dangling_spur.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_permissive_shared_dangling_spur.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_permissive_shared_dangling_spur.g
      -P ${AP214_TEST_DIR}/ap214_permissive_shared_dangling_spur.cmake
  )
  brlcad_add_test(
    NAME ap214_permissive_same_vertex_spline_spur
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_permissive_same_vertex_spline_spur.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_permissive_same_vertex_spline_spur.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_permissive_same_vertex_spline_spur.g
      -DWIDE_INPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_permissive_same_vertex_spline_spur_wide.stp
      -DWIDE_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_permissive_same_vertex_spline_spur_wide.json
      -P ${AP214_TEST_DIR}/ap214_permissive_same_vertex_spline_spur.cmake
  )
  brlcad_add_test(
    NAME ap214_exact_distinct_edge_zero_area_face
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_exact_distinct_edge_zero_area_face.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_exact_distinct_edge_zero_area_face.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_exact_distinct_edge_zero_area_face.g
      -P ${AP214_TEST_DIR}/ap214_exact_distinct_edge_zero_area_face.cmake
  )
  brlcad_add_test(
    NAME ap214_source_first_near_cylinder_line
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_source_first_near_cylinder_line.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_source_first_near_cylinder_line.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_source_first_near_cylinder_line.g
      -P ${AP214_TEST_DIR}/ap214_source_first_near_cylinder_line.cmake
  )
  brlcad_add_test(
    NAME ap214_permissive_periodic_boundary_inference
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_permissive_periodic_boundary_inference.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_permissive_periodic_boundary_inference.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_permissive_periodic_boundary_inference.g
      -P ${AP214_TEST_DIR}/ap214_permissive_periodic_boundary_inference.cmake
  )
  brlcad_add_test(
    NAME ap214_measured_periodic_closure_band
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_measured_periodic_closure_band.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_measured_periodic_closure_band.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_measured_periodic_closure_band.g
      -DEXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_measured_periodic_closure_band_exact.json
      -DEXACT_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_measured_periodic_closure_band_exact.g
      -P ${AP214_TEST_DIR}/ap214_measured_periodic_closure_band.cmake
  )
  brlcad_add_test(
    NAME ap214_open_nurbs_measured_mismatch
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_open_nurbs_measured_mismatch.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_open_nurbs_measured_mismatch.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_open_nurbs_measured_mismatch.g
      -DEXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_open_nurbs_measured_mismatch_exact.json
      -P ${AP214_TEST_DIR}/ap214_open_nurbs_measured_mismatch.cmake
  )
  brlcad_add_test(
    NAME ap214_repeated_distinct_surface_knots
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_repeated_distinct_surface_knots.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_repeated_distinct_surface_knots.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_repeated_distinct_surface_knots.g
      -DEXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_repeated_distinct_surface_knots_exact.json
      -P ${AP214_TEST_DIR}/ap214_repeated_distinct_surface_knots.cmake
  )
  brlcad_add_test(
    NAME ap214_proven_positive_rational_denominator
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_proven_positive_rational_denominator.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_proven_positive_rational_denominator.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_proven_positive_rational_denominator.g
      -DEXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_proven_positive_rational_denominator_exact.json
      -P ${AP214_TEST_DIR}/ap214_proven_positive_rational_denominator.cmake
  )
  brlcad_add_test(
    NAME ap214_endpoint_anchored_surface_tolerance
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_endpoint_anchored_surface_tolerance.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_endpoint_anchored_surface_tolerance.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_endpoint_anchored_surface_tolerance.g
      -DEXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_endpoint_anchored_surface_tolerance_exact.json
      -DEXACT_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_endpoint_anchored_surface_tolerance_exact.g
      -P ${AP214_TEST_DIR}/ap214_endpoint_anchored_surface_tolerance.cmake
  )
  brlcad_add_test(
    NAME ap214_collapsed_topology_edge
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_collapsed_topology_edge.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_collapsed_topology_edge.json
      -DEXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_collapsed_topology_edge_exact.json
      -P ${AP214_TEST_DIR}/ap214_collapsed_topology_edge.cmake
  )
  brlcad_add_test(
    NAME ap214_same_vertex_line_edge
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_same_vertex_line_edge.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_same_vertex_line_edge.json
      -DEXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_same_vertex_line_edge_exact.json
      -DREJECT_INPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_same_vertex_line_edge_reject.stp
      -DREJECT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_same_vertex_line_edge_reject.json
      -DZERO_INPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_same_vertex_line_edge_zero.stp
      -DZERO_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_same_vertex_line_edge_zero.json
      -DZERO_EXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_same_vertex_line_edge_zero_exact.json
      -P ${AP214_TEST_DIR}/ap214_same_vertex_line_edge.cmake
  )
  brlcad_add_test(
    NAME ap214_closed_curve_endpoint_interval
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_closed_curve_endpoint_interval.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_closed_curve_endpoint_interval.json
      -DEXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_closed_curve_endpoint_interval_exact.json
      -P ${AP214_TEST_DIR}/ap214_closed_curve_endpoint_interval.cmake
  )
  brlcad_add_test(
    NAME ap214_closed_curve_interior_seam
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_closed_curve_interior_seam.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_closed_curve_interior_seam.json
      -DEXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_closed_curve_interior_seam_exact.json
      -P ${AP214_TEST_DIR}/ap214_closed_curve_interior_seam.cmake
  )
  brlcad_add_test(
    NAME ap214_closed_curve_near_native_seam
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_closed_curve_near_native_seam.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_closed_curve_near_native_seam.json
      -DEXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_closed_curve_near_native_seam_exact.json
      -P ${AP214_TEST_DIR}/ap214_closed_curve_near_native_seam.cmake
  )
  brlcad_add_test(
    NAME ap214_eccentric_ellipse_arc
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_eccentric_ellipse_arc.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_eccentric_ellipse_arc.json
      -DEXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_eccentric_ellipse_arc_exact.json
      -P ${AP214_TEST_DIR}/ap214_eccentric_ellipse_arc.cmake
  )
  brlcad_add_test(
    NAME ap214_nurbs_terminal_extension
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_nurbs_terminal_extension.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_nurbs_terminal_extension.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_nurbs_terminal_extension.g
      -DEXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_nurbs_terminal_extension_exact.json
      -DREJECT_INPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_nurbs_terminal_extension_reject.stp
      -DREJECT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_nurbs_terminal_extension_reject.json
      -P ${AP214_TEST_DIR}/ap214_nurbs_terminal_extension.cmake
  )
  brlcad_add_test(
    NAME ap214_two_side_singular_corner
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_two_side_singular_corner.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_two_side_singular_corner.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_two_side_singular_corner.g
      -DEXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_two_side_singular_corner_exact.json
      -DEXACT_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_two_side_singular_corner_exact.g
      -P ${AP214_TEST_DIR}/ap214_two_side_singular_corner.cmake
  )
  brlcad_add_test(
    NAME ap214_spherical_band_inner_first
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_spherical_band_inner_first.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_spherical_band_inner_first.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_spherical_band_inner_first.g
      -P ${AP214_TEST_DIR}/ap214_spherical_band_inner_first.cmake
  )
  brlcad_add_test(
    NAME ap214_two_arc_sphere
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_two_arc_sphere.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_two_arc_sphere.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_two_arc_sphere.g
      -P ${AP214_TEST_DIR}/ap214_two_arc_sphere.cmake
  )
  brlcad_add_test(
    NAME ap214_shifted_two_arc_sphere
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_shifted_two_arc_sphere.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_shifted_two_arc_sphere.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_shifted_two_arc_sphere.g
      -P ${AP214_TEST_DIR}/ap214_shifted_two_arc_sphere.cmake
  )
  brlcad_add_test(
    NAME ap214_faceted_brep
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_faceted_brep.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_faceted_brep.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_faceted_brep.g
      -P ${AP214_TEST_DIR}/ap214_faceted_brep.cmake
  )
  brlcad_add_test(
    NAME ap214_mapped_items
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_mapped_items.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_mapped_items.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_mapped_items.g
      -P ${AP214_TEST_DIR}/ap214_mapped_items.cmake
  )
  brlcad_add_test(
    NAME ap214_brep_with_voids
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_brep_with_voids.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_brep_with_voids.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_brep_with_voids.g
      -P ${AP214_TEST_DIR}/ap214_brep_with_voids.cmake
  )
  brlcad_add_test(
    NAME ap214_csg
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_csg.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_csg.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_csg.g
      -P ${AP214_TEST_DIR}/ap214_csg.cmake
  )
  brlcad_add_test(
    NAME ap214_structured_units
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DG_STEP=$<TARGET_FILE:g-step>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_structured_units.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_structured_units.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_structured_units.g
      -DEXPORT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_structured_units_export.json
      -DEXPORT_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_structured_units_roundtrip.stp
      -P ${AP214_TEST_DIR}/ap214_structured_units.cmake
  )
  brlcad_add_test(
    NAME ap214_csg_invalid
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_csg_invalid.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_csg_invalid.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_csg_invalid.g
      -DSTRICT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_csg_invalid_strict.json
      -DSTRICT_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_csg_invalid_strict.g
      -P ${AP214_TEST_DIR}/ap214_csg_invalid.cmake
  )
  brlcad_add_test(
    NAME ap214_wireframe
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_wireframe.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_wireframe.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_wireframe.g
      -P ${AP214_TEST_DIR}/ap214_wireframe.cmake
  )
  brlcad_add_test(
    NAME ap214_geometric_surface_set
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_geometric_surface_set.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_geometric_surface_set.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_geometric_surface_set.g
      -P ${AP214_TEST_DIR}/ap214_geometric_surface_set.cmake
  )
  brlcad_add_test(
    NAME ap214_composite_curve
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_composite_curve.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_composite_curve.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_composite_curve.g
      -P ${AP214_TEST_DIR}/ap214_composite_curve.cmake
  )
  brlcad_add_test(
    NAME ap214_extruded_area
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_extruded_area.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_extruded_area.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_extruded_area.g
      -P ${AP214_TEST_DIR}/ap214_extruded_area.cmake
  )
  brlcad_add_test(
    NAME ap214_extruded_face
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_extruded_face.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_extruded_face.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_extruded_face.g
      -P ${AP214_TEST_DIR}/ap214_extruded_face.cmake
  )
  brlcad_add_test(
    NAME ap214_repair_policy
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_repair_policy.stp
      -DOUTPUT_SAFE=${CMAKE_CURRENT_BINARY_DIR}/ap214_repair_safe.g
      -DOUTPUT_NONE=${CMAKE_CURRENT_BINARY_DIR}/ap214_repair_none.g
      -DREPORT_SAFE=${CMAKE_CURRENT_BINARY_DIR}/ap214_repair_safe.json
      -DREPORT_NONE=${CMAKE_CURRENT_BINARY_DIR}/ap214_repair_none.json
      -P ${AP214_TEST_DIR}/ap214_repair_policy.cmake
  )
  brlcad_add_test(
    NAME ap214_per_representation_tolerance
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_per_representation_tolerance.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_per_representation_tolerance.json
      -DOVERRIDE_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_per_representation_tolerance_override.json
      -P ${AP214_TEST_DIR}/ap214_per_representation_tolerance.cmake
  )
  brlcad_add_test(
    NAME ap214_representation_tolerance_periodic_cap
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_representation_tolerance_periodic_cap.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_representation_tolerance_periodic_cap.json
      -DOVERRIDE_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_representation_tolerance_periodic_cap_override.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_representation_tolerance_periodic_cap.g
      -P ${AP214_TEST_DIR}/ap214_representation_tolerance_periodic_cap.cmake
  )
  brlcad_add_test(
    NAME ap214_revolved_area
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DNIRT=$<TARGET_FILE:nirt>
      -DINPUT=${AP214_TEST_DIR}/ap214_revolved_area.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_revolved_area.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_revolved_area.g
      -P ${AP214_TEST_DIR}/ap214_revolved_area.cmake
  )
  brlcad_add_test(
    NAME ap214_revolved_face
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_revolved_face.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_revolved_face.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_revolved_face.g
      -P ${AP214_TEST_DIR}/ap214_revolved_face.cmake
  )
  brlcad_add_test(
    NAME ap214_swept_disk
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DNIRT=$<TARGET_FILE:nirt>
      -DINPUT=${AP214_TEST_DIR}/ap214_swept_disk.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_swept_disk.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_swept_disk.g
      -P ${AP214_TEST_DIR}/ap214_swept_disk.cmake
  )
  brlcad_add_test(
    NAME ap214_solid_replica
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${AP214_TEST_DIR}/ap214_solid_replica.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_solid_replica.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_solid_replica.g
      -P ${AP214_TEST_DIR}/ap214_solid_replica.cmake
  )
  brlcad_add_test(
    NAME ap214_solid_replica_cycle
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DINPUT=${AP214_TEST_DIR}/ap214_solid_replica_cycle.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_solid_replica_cycle.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_solid_replica_cycle.g
      -P ${AP214_TEST_DIR}/ap214_solid_replica_cycle.cmake
  )
endif()

cmakefiles(
  ${ap214g_SOURCES}
  ${gap214_SOURCES}
  AP214e3.h
  AP214Adapter.h
  ../STEPLazySession.cpp
  ../STEPLazySession.h
  AP214Presentation.h
  CMakeLists.txt
  ExportComb.cpp
  ExportTree.cpp
  ap214e3.exp
  tests/ap214_brep_with_voids.cmake
  tests/ap214_brep_with_voids.stp
  tests/ap214_budget_controls.cmake
  tests/ap214_csg.cmake
  tests/ap214_csg.stp
  tests/ap214_csg_invalid.cmake
  tests/ap214_csg_invalid.stp
  tests/ap214_composite_curve.cmake
  tests/ap214_composite_curve.stp
  tests/ap214_conical_closed_edge_band.cmake
  tests/ap214_conical_closed_edge_band.stp
  tests/ap214_entity_selection.cmake
  tests/ap214_shared_conical_surface_bands.cmake
  tests/ap214_shared_conical_surface_bands.stp
  tests/ap214_legacy_contexts.stp
  tests/ap214_multi_edge_conical_cap.cmake
  tests/ap214_multi_edge_conical_cap.stp
  tests/ap214_measured_full_period_chain.cmake
  tests/ap214_measured_full_period_chain.stp
  tests/ap214_four_face_keyhole.cmake
  tests/ap214_four_face_keyhole.stp
  tests/ap214_paired_singleton_keyhole.stp
  tests/ap214_neighbor_keyhole_seam.stp
  tests/ap214_singleton_keyhole_solid.stp
  tests/ap214_selected_solid_regression.cmake
  tests/ap214_spherical_antipodal_boundary.stp
  tests/ap214_spherical_antipodal_boundary_with_hole.stp
  tests/ap214_structural_repair_fixture.cmake
  tests/ap214_periodic_face_branch.cmake
  tests/ap214_periodic_face_branch.stp
  tests/ap214_doubly_periodic_chain.cmake
  tests/ap214_doubly_periodic_chain.stp
  tests/ap214_contractible_periodic_neighbor.cmake
  tests/ap214_contractible_periodic_neighbor.stp
  tests/ap214_measured_native_periodic_boundary.cmake
  tests/ap214_measured_native_periodic_boundary.stp
  tests/ap214_measured_periodic_winding.cmake
  tests/ap214_measured_periodic_winding.stp
  tests/ap214_nonmanifold_four_use_edge.cmake
  tests/ap214_nonmanifold_four_use_edge.stp
  tests/ap214_periodic_endpoint_branch.cmake
  tests/ap214_periodic_endpoint_branch.stp
  tests/ap214_periodic_band_with_hole.cmake
  tests/ap214_periodic_band_with_hole.stp
  tests/ap214_shared_periodic_edge.cmake
  tests/ap214_shared_periodic_edge.stp
  tests/ap214_shared_revolution_edge.cmake
  tests/ap214_shared_revolution_edge.stp
  tests/ap214_segmented_cylinder_band.cmake
  tests/ap214_segmented_cylinder_band.stp
  tests/ap214_wavy_periodic_band.cmake
  tests/ap214_wavy_periodic_band.stp
  tests/ap214_nurbs_native_seam.cmake
  tests/ap214_nurbs_native_seam.stp
  tests/ap214_collapsed_wavy_periodic_band.cmake
  tests/ap214_collapsed_wavy_periodic_band.stp
  tests/ap214_offset_vertex_periodic_band.cmake
  tests/ap214_offset_vertex_periodic_band.stp
  tests/ap214_periodic_edge_propagation.cmake
  tests/ap214_periodic_edge_propagation.stp
  tests/ap214_measured_vertex_tolerance_band.cmake
  tests/ap214_measured_vertex_tolerance_band.stp
  tests/ap214_open_spline_vertex_tolerance.cmake
  tests/ap214_open_spline_vertex_tolerance.stp
  tests/ap214_permissive_inferred_curve.cmake
  tests/ap214_permissive_inferred_curve.stp
  tests/ap214_offset_extrusion.cmake
  tests/ap214_offset_extrusion.stp
  tests/ap214_offset_revolution.cmake
  tests/ap214_offset_revolution.stp
  tests/ap214_permissive_shared_dangling_spur.cmake
  tests/ap214_permissive_shared_dangling_spur.stp
  tests/ap214_permissive_same_vertex_spline_spur.cmake
  tests/ap214_permissive_same_vertex_spline_spur.stp
  tests/ap214_exact_distinct_edge_zero_area_face.cmake
  tests/ap214_exact_distinct_edge_zero_area_face.stp
  tests/ap214_source_first_near_cylinder_line.cmake
  tests/ap214_source_first_near_cylinder_line.stp
  tests/ap214_permissive_periodic_boundary_inference.cmake
  tests/ap214_permissive_periodic_boundary_inference.stp
  tests/ap214_measured_periodic_closure_band.cmake
  tests/ap214_measured_periodic_closure_band.stp
  tests/ap214_open_nurbs_measured_mismatch.cmake
  tests/ap214_open_nurbs_measured_mismatch.stp
  tests/ap214_repeated_distinct_surface_knots.cmake
  tests/ap214_repeated_distinct_surface_knots.stp
  tests/ap214_proven_positive_rational_denominator.cmake
  tests/ap214_proven_positive_rational_denominator.stp
  tests/ap214_endpoint_anchored_surface_tolerance.cmake
  tests/ap214_endpoint_anchored_surface_tolerance.stp
  tests/ap214_collapsed_torus_seam.cmake
  tests/ap214_collapsed_torus_seam.stp
  tests/ap214_collapsed_topology_edge.cmake
  tests/ap214_collapsed_topology_edge.stp
  tests/ap214_same_vertex_line_edge.cmake
  tests/ap214_same_vertex_line_edge.stp
  tests/ap214_closed_curve_endpoint_interval.cmake
  tests/ap214_closed_curve_endpoint_interval.stp
  tests/ap214_closed_curve_interior_seam.cmake
  tests/ap214_closed_curve_interior_seam.stp
  tests/ap214_closed_curve_near_native_seam.cmake
  tests/ap214_closed_curve_near_native_seam.stp
  tests/ap214_eccentric_ellipse_arc.cmake
  tests/ap214_eccentric_ellipse_arc.stp
  tests/ap214_nurbs_terminal_extension.cmake
  tests/ap214_nurbs_terminal_extension.stp
  tests/ap214_two_side_singular_corner.cmake
  tests/ap214_two_side_singular_corner.stp
  tests/ap214_degenerate_torus_endpoint.cmake
  tests/ap214_degenerate_torus_endpoint.stp
  tests/ap214_horn_torus_keyhole.cmake
  tests/ap214_horn_torus_keyhole.stp
  tests/ap214_degenerate_torus_sheet.cmake
  tests/ap214_degenerate_torus_sheet.stp
  tests/ap214_direct_manifold.cmake
  tests/ap214_direct_manifold.stp
  tests/ap214_missing_planar_triangular_cap.cmake
  tests/ap214_repeated_vertex_periodic_lobe.cmake
  tests/ap214_repeated_vertex_periodic_lobe.stp
  tests/ap214_deterministic_output.cmake
  tests/ap214_dry_run.cmake
  tests/ap214_extruded_area.cmake
  tests/ap214_extruded_area.stp
  tests/ap214_extruded_face.cmake
  tests/ap214_extruded_face.stp
  tests/ap214_keyhole_loop.cmake
  tests/ap214_keyhole_loop.stp
  tests/ap214_lazy_diagnostics.stp
  tests/ap214_lazy_diagnostics.cmake
  tests/ap214_lazy_many_solids.cmake
  tests/ap214_lazy_many_solids.stp
  tests/ap214_faceted_brep.cmake
  tests/ap214_faceted_brep.stp
  tests/ap214_face_wr2_off_surface_torus_invalid.cmake
  tests/ap214_face_wr2_off_surface_torus_invalid.stp
  tests/ap214_mapped_items.cmake
  tests/ap214_mapped_items.stp
  tests/ap214_notched_periodic_band.cmake
  tests/ap214_notched_periodic_band.stp
  tests/ap214_negative_spindle_torus.cmake
  tests/ap214_negative_spindle_torus.stp
  tests/ap214_spherical_band_inner_first.cmake
  tests/ap214_spherical_band_inner_first.stp
  tests/ap214_parallel_determinism.cmake
  tests/ap214_parallel_failure.cmake
  tests/ap214_parallel_failure.stp
  tests/ap214_missing_face_reference.cmake
  tests/ap214_periodic_torus.cmake
  tests/ap214_periodic_torus.stp
  tests/ap214_vertex_loop_torus.cmake
  tests/ap214_vertex_loop_torus.stp
  tests/ap214_vertex_loop_sphere.cmake
  tests/ap214_vertex_loop_sphere.stp
  tests/ap214_vertex_loop_conical_face.cmake
  tests/ap214_vertex_loop_conical_face.stp
  tests/ap214_vertex_loop_spline_pole.cmake
  tests/ap214_vertex_loop_spline_pole.stp
  tests/ap214_repair_policy.cmake
  tests/ap214_repair_policy.stp
  tests/ap214_per_representation_tolerance.cmake
  tests/ap214_per_representation_tolerance.stp
  tests/ap214_representation_tolerance_periodic_cap.cmake
  tests/ap214_representation_tolerance_periodic_cap.stp
  tests/ap214_revolved_area.cmake
  tests/ap214_revolved_area.stp
  tests/ap214_revolved_face.cmake
  tests/ap214_revolved_face.stp
  tests/ap214_swept_disk.cmake
  tests/ap214_swept_disk.stp
  tests/ap214_solid_replica.cmake
  tests/ap214_solid_replica.stp
  tests/ap214_solid_replica_cycle.cmake
  tests/ap214_solid_replica_cycle.stp
  tests/ap214_short_cylinder_seam.cmake
  tests/ap214_single_winding_cylinder_boundary_invalid.cmake
  tests/ap214_single_winding_cylinder_boundary_invalid.stp
  tests/ap214_duplicate_periodic_seam.cmake
  tests/ap214_duplicate_periodic_seam.stp
  tests/ap214_distinct_edge_keyhole_rejoin.cmake
  tests/ap214_distinct_edge_keyhole_rejoin.stp
  tests/ap214_sub_tolerance_torus_seam.cmake
  tests/ap214_sub_tolerance_torus_seam.stp
  tests/ap214_short_cylinder_seam.stp
  tests/ap214_shifted_two_arc_sphere.cmake
  tests/ap214_shifted_two_arc_sphere.stp
  tests/ap214_two_arc_sphere.cmake
  tests/ap214_two_arc_sphere.stp
  tests/ap214_import_test.cpp
  tests/ap214_brep_transaction_test.cpp
  tests/ap214_lazy_session_test.cpp
  tests/ap214_style_layer.stp
  tests/ap214_style_report.cmake
  tests/ap214_structured_units.cmake
  tests/ap214_structured_units.stp
  tests/ap214_surface_shells.cmake
  tests/ap214_surface_shells.stp
  tests/ap214_wireframe.cmake
  tests/ap214_wireframe.stp
  tests/ap214_geometric_surface_set.cmake
  tests/ap214_geometric_surface_set.stp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
