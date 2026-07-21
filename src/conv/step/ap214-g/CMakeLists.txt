set(STEP_SCHEMA_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ap214e3.exp)
generate_schema_inputs(${STEP_SCHEMA_FILE} ap214g)

set(
  ap214g_SOURCES
  ../BRLCADWrapper.cpp
  ../STEPString.cpp
  ../STEPWrapper.cpp
  AP214Adapter.cpp
  AP214CSG.cpp
  AP214Metadata.cpp
  AP214Presentation.cpp
  AP214SweptSolid.cpp
  ap214-g.cpp
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

if(BRLCAD_ENABLE_STEP)
  add_library(ap214_schema_bindings STATIC ${ap214g_express_srcs})
  target_include_directories(ap214_schema_bindings BEFORE PRIVATE ${AP214_INCLUDE_DIRS})
  target_compile_definitions(ap214_schema_bindings PRIVATE BRLCADBUILD HAVE_CONFIG_H SC_SCHEMA_DLL_EXPORTS)
  add_dependencies(ap214_schema_bindings ${ap214g_SCHEMA_TARGET})
  set_target_properties(ap214_schema_bindings PROPERTIES FOLDER "BRL-CAD Static Libraries/STEP")
  if(COMMAND target_precompile_headers)
    target_precompile_headers(ap214_schema_bindings PRIVATE "${ap214g_PRECOMPILE_HEADER}")
  endif()

  brlcad_addexec(ap214-g "${ap214g_SOURCES}" "ap214_schema_bindings;${ap214g_LIBS}" NO_STRICT FOLDER Conv
    UNITY_BUILD_CODE_BEFORE_INCLUDE "#undef CLASSNAME\n#undef ENTITYNAME"
  )
  target_include_directories(ap214-g BEFORE PRIVATE ${AP214_INCLUDE_DIRS})
  target_compile_definitions(ap214-g PRIVATE AP214e3 SC_SCHEMA_DLL_EXPORTS)
  if(STEPCODE_LAZY_FOUND)
    target_compile_definitions(ap214-g PRIVATE HAVE_STEPCODE_LAZY)
  endif()
  add_dependencies(ap214-g ${ap214g_SCHEMA_TARGET})

  brlcad_addexec(
    ap214_import_test
    "tests/ap214_import_test.cpp;AP214Adapter.cpp;../STEPString.cpp"
    "libbu"
    TEST
  )
  target_include_directories(ap214_import_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)
  brlcad_add_test(NAME ap214_import_adapter COMMAND ap214_import_test)
  if(STEPCODE_LAZY_FOUND)
    brlcad_addexec(
      ap214_lazy_session_test
      "tests/ap214_lazy_session_test.cpp;../STEPLazySession.cpp"
      "ap214_schema_bindings;${STEPCODE_LAZY_LIBRARY};${STEPCODE_EDITOR_LIBRARY};${STEPCODE_DAI_LIBRARY};${STEPCODE_CORE_LIBRARY};${STEPCODE_UTILS_LIBRARY}"
      TEST
    )
    target_include_directories(ap214_lazy_session_test BEFORE PRIVATE ${AP214_INCLUDE_DIRS})
    target_compile_definitions(ap214_lazy_session_test PRIVATE AP214e3 SC_SCHEMA_DLL_EXPORTS)
    add_dependencies(ap214_lazy_session_test ${ap214g_SCHEMA_TARGET})
    brlcad_add_test(
      NAME ap214_lazy_session
      COMMAND ap214_lazy_session_test
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_direct_manifold.stp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_lazy_diagnostics.stp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_legacy_contexts.stp
    )
  endif()
  brlcad_add_test(
    NAME ap214_style_metadata
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_style_layer.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_style_layer.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_style_layer.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_style_report.cmake
  )
  brlcad_add_test(
    NAME ap214_dry_run
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_direct_manifold.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_dry_run.json
      -DSUMMARY=${CMAKE_CURRENT_BINARY_DIR}/ap214_dry_run.csv
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_dry_run.cmake
  )
  brlcad_add_test(
    NAME ap214_lazy_diagnostics
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_lazy_diagnostics.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_lazy_diagnostics.json
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_lazy_diagnostics.cmake
  )
  brlcad_add_test(
    NAME ap214_lazy_many_solids
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_lazy_many_solids.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_lazy_many_solids.json
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_lazy_many_solids.cmake
  )
  brlcad_add_test(
    NAME ap214_entity_selection
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_lazy_many_solids.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_entity_selection.json
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_entity_selection.cmake
  )
  brlcad_add_test(
    NAME ap214_deterministic_output
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_direct_manifold.stp
      -DOUTPUT_A=${CMAKE_CURRENT_BINARY_DIR}/ap214_deterministic_a.g
      -DOUTPUT_B=${CMAKE_CURRENT_BINARY_DIR}/ap214_deterministic_b.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_deterministic_output.cmake
  )
  brlcad_add_test(
    NAME ap214_parallel_determinism
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_brep_with_voids.stp
      -DOUTPUT_SERIAL=${CMAKE_CURRENT_BINARY_DIR}/ap214_parallel_serial.g
      -DOUTPUT_PARALLEL=${CMAKE_CURRENT_BINARY_DIR}/ap214_parallel_four.g
      -DREPORT_SERIAL=${CMAKE_CURRENT_BINARY_DIR}/ap214_parallel_serial.json
      -DREPORT_PARALLEL=${CMAKE_CURRENT_BINARY_DIR}/ap214_parallel_four.json
      -DEXPECTED_GEOMETRY=2
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_parallel_determinism.cmake
  )
  brlcad_add_test(
    NAME ap214_direct_parallel_determinism
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_direct_manifold.stp
      -DOUTPUT_SERIAL=${CMAKE_CURRENT_BINARY_DIR}/ap214_direct_parallel_serial.g
      -DOUTPUT_PARALLEL=${CMAKE_CURRENT_BINARY_DIR}/ap214_direct_parallel_four.g
      -DREPORT_SERIAL=${CMAKE_CURRENT_BINARY_DIR}/ap214_direct_parallel_serial.json
      -DREPORT_PARALLEL=${CMAKE_CURRENT_BINARY_DIR}/ap214_direct_parallel_four.json
      -DEXPECTED_GEOMETRY=1
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_parallel_determinism.cmake
  )
  brlcad_add_test(
    NAME ap214_parallel_failure
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_parallel_failure.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_parallel_failure.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_parallel_failure.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_parallel_failure.cmake
  )
  brlcad_add_test(
    NAME ap214_direct_manifold
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_direct_manifold.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_direct_manifold.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_direct_manifold.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_direct_manifold.cmake
  )
  brlcad_add_test(
    NAME ap214_surface_shells
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_surface_shells.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_surface_shells.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_surface_shells.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_surface_shells.cmake
  )
  brlcad_add_test(
    NAME ap214_keyhole_loop
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_keyhole_loop.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_keyhole_loop.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_keyhole_loop.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_keyhole_loop.cmake
  )
  brlcad_add_test(
    NAME ap214_periodic_torus
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_periodic_torus.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_periodic_torus.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_periodic_torus.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_periodic_torus.cmake
  )
  brlcad_add_test(
    NAME ap214_sub_tolerance_torus_seam
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_sub_tolerance_torus_seam.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_sub_tolerance_torus_seam.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_sub_tolerance_torus_seam.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_sub_tolerance_torus_seam.cmake
  )
  brlcad_add_test(
    NAME ap214_negative_spindle_torus
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_negative_spindle_torus.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_negative_spindle_torus.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_negative_spindle_torus.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_negative_spindle_torus.cmake
  )
  brlcad_add_test(
    NAME ap214_collapsed_torus_seam
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_collapsed_torus_seam.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_collapsed_torus_seam.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_collapsed_torus_seam.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_collapsed_torus_seam.cmake
  )
  brlcad_add_test(
    NAME ap214_short_cylinder_seam
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_short_cylinder_seam.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_short_cylinder_seam.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_short_cylinder_seam.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_short_cylinder_seam.cmake
  )
  brlcad_add_test(
    NAME ap214_notched_periodic_band
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_notched_periodic_band.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_notched_periodic_band.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_notched_periodic_band.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_notched_periodic_band.cmake
  )
  brlcad_add_test(
    NAME ap214_conical_closed_edge_band
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_conical_closed_edge_band.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_conical_closed_edge_band.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_conical_closed_edge_band.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_conical_closed_edge_band.cmake
  )
  brlcad_add_test(
    NAME ap214_segmented_cylinder_band
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_segmented_cylinder_band.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_segmented_cylinder_band.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_segmented_cylinder_band.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_segmented_cylinder_band.cmake
  )
  brlcad_add_test(
    NAME ap214_wavy_periodic_band
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_wavy_periodic_band.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_wavy_periodic_band.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_wavy_periodic_band.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_wavy_periodic_band.cmake
  )
  brlcad_add_test(
    NAME ap214_spherical_band_inner_first
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_spherical_band_inner_first.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_spherical_band_inner_first.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_spherical_band_inner_first.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_spherical_band_inner_first.cmake
  )
  brlcad_add_test(
    NAME ap214_two_arc_sphere
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_two_arc_sphere.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_two_arc_sphere.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_two_arc_sphere.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_two_arc_sphere.cmake
  )
  brlcad_add_test(
    NAME ap214_shifted_two_arc_sphere
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_shifted_two_arc_sphere.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_shifted_two_arc_sphere.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_shifted_two_arc_sphere.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_shifted_two_arc_sphere.cmake
  )
  brlcad_add_test(
    NAME ap214_faceted_brep
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_faceted_brep.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_faceted_brep.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_faceted_brep.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_faceted_brep.cmake
  )
  brlcad_add_test(
    NAME ap214_mapped_items
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_mapped_items.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_mapped_items.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_mapped_items.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_mapped_items.cmake
  )
  brlcad_add_test(
    NAME ap214_brep_with_voids
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_brep_with_voids.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_brep_with_voids.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_brep_with_voids.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_brep_with_voids.cmake
  )
  brlcad_add_test(
    NAME ap214_csg
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_csg.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_csg.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_csg.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_csg.cmake
  )
  brlcad_add_test(
    NAME ap214_csg_invalid
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_csg_invalid.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_csg_invalid.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_csg_invalid.g
      -DSTRICT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_csg_invalid_strict.json
      -DSTRICT_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_csg_invalid_strict.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_csg_invalid.cmake
  )
  brlcad_add_test(
    NAME ap214_wireframe
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_wireframe.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_wireframe.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_wireframe.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_wireframe.cmake
  )
  brlcad_add_test(
    NAME ap214_composite_curve
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_composite_curve.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_composite_curve.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_composite_curve.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_composite_curve.cmake
  )
  brlcad_add_test(
    NAME ap214_extruded_area
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_extruded_area.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_extruded_area.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_extruded_area.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_extruded_area.cmake
  )
  brlcad_add_test(
    NAME ap214_extruded_face
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_extruded_face.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_extruded_face.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_extruded_face.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_extruded_face.cmake
  )
  brlcad_add_test(
    NAME ap214_repair_policy
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_repair_policy.stp
      -DOUTPUT_SAFE=${CMAKE_CURRENT_BINARY_DIR}/ap214_repair_safe.g
      -DOUTPUT_NONE=${CMAKE_CURRENT_BINARY_DIR}/ap214_repair_none.g
      -DREPORT_SAFE=${CMAKE_CURRENT_BINARY_DIR}/ap214_repair_safe.json
      -DREPORT_NONE=${CMAKE_CURRENT_BINARY_DIR}/ap214_repair_none.json
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_repair_policy.cmake
  )
  brlcad_add_test(
    NAME ap214_revolved_area
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DNIRT=$<TARGET_FILE:nirt>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_revolved_area.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_revolved_area.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_revolved_area.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_revolved_area.cmake
  )
  brlcad_add_test(
    NAME ap214_revolved_face
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_revolved_face.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_revolved_face.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_revolved_face.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_revolved_face.cmake
  )
  brlcad_add_test(
    NAME ap214_swept_disk
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DNIRT=$<TARGET_FILE:nirt>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_swept_disk.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_swept_disk.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_swept_disk.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_swept_disk.cmake
  )
  brlcad_add_test(
    NAME ap214_solid_replica
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_solid_replica.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_solid_replica.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_solid_replica.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_solid_replica.cmake
  )
  brlcad_add_test(
    NAME ap214_solid_replica_cycle
    COMMAND
      ${CMAKE_COMMAND}
      -DAP214_G=$<TARGET_FILE:ap214-g>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_solid_replica_cycle.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap214_solid_replica_cycle.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap214_solid_replica_cycle.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap214_solid_replica_cycle.cmake
  )
endif()

cmakefiles(
  ${ap214g_SOURCES}
  AP214Adapter.h
  AP214CSG.h
  ../STEPLazySession.cpp
  ../STEPLazySession.h
  AP214Metadata.h
  AP214Presentation.h
  AP214SweptSolid.h
  CMakeLists.txt
  ap214e3.exp
  tests/ap214_brep_with_voids.cmake
  tests/ap214_brep_with_voids.stp
  tests/ap214_csg.cmake
  tests/ap214_csg.stp
  tests/ap214_csg_invalid.cmake
  tests/ap214_csg_invalid.stp
  tests/ap214_composite_curve.cmake
  tests/ap214_composite_curve.stp
  tests/ap214_conical_closed_edge_band.cmake
  tests/ap214_conical_closed_edge_band.stp
  tests/ap214_segmented_cylinder_band.cmake
  tests/ap214_segmented_cylinder_band.stp
  tests/ap214_wavy_periodic_band.cmake
  tests/ap214_wavy_periodic_band.stp
  tests/ap214_collapsed_torus_seam.cmake
  tests/ap214_collapsed_torus_seam.stp
  tests/ap214_direct_manifold.cmake
  tests/ap214_direct_manifold.stp
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
  tests/ap214_periodic_torus.cmake
  tests/ap214_periodic_torus.stp
  tests/ap214_repair_policy.cmake
  tests/ap214_repair_policy.stp
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
  tests/ap214_sub_tolerance_torus_seam.cmake
  tests/ap214_sub_tolerance_torus_seam.stp
  tests/ap214_short_cylinder_seam.stp
  tests/ap214_shifted_two_arc_sphere.cmake
  tests/ap214_shifted_two_arc_sphere.stp
  tests/ap214_two_arc_sphere.cmake
  tests/ap214_two_arc_sphere.stp
  tests/ap214_import_test.cpp
  tests/ap214_lazy_session_test.cpp
  tests/ap214_style_layer.stp
  tests/ap214_style_report.cmake
  tests/ap214_surface_shells.cmake
  tests/ap214_surface_shells.stp
  tests/ap214_wireframe.cmake
  tests/ap214_wireframe.stp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
