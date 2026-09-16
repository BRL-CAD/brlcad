link_directories(${CMAKE_BINARY_DIR}/${LIB_DIR})

set(STEP_SCHEMA_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ap203.exp)

generate_schema_inputs(${STEP_SCHEMA_FILE} stepg ap203)

set(
  stepg_SOURCES
  ${stepg_express_srcs}
  ../BRLCADWrapper.cpp
  ../STEPString.cpp
  ../STEPBudget.cpp
  ../STEPBrepValidation.cpp
  ../STEPWrapper.cpp
  ../STEPWrapperAttributes.cpp
  ../STEPWrapperIO.cpp
  AdvancedBrepShapeRepresentation.cpp
  AdvancedFace.cpp
  AmountOfSubstanceContextDependentUnit.cpp
  AmountOfSubstanceConversionBasedUnit.cpp
  AmountOfSubstanceSiUnit.cpp
  AmountOfSubstanceUnit.cpp
  ApplicationContext.cpp
  ApplicationContextElement.cpp
  AreaContextDependentUnit.cpp
  AreaConversionBasedUnit.cpp
  AreaSiUnit.cpp
  AreaUnit.cpp
  AssemblyComponentUsage.cpp
  Axis1Placement.cpp
  Axis2Placement.cpp
  Axis2Placement2D.cpp
  Axis2Placement3D.cpp
  BSplineCurve.cpp
  BSplineCurveWithKnots.cpp
  BSplineSurface.cpp
  BSplineSurfaceWithKnots.cpp
  BezierCurve.cpp
  BezierSurface.cpp
  BoundaryCurve.cpp
  BoundedCurve.cpp
  BoundedPCurve.cpp
  BoundedSurface.cpp
  BoundedSurfaceCurve.cpp
  BrepWithVoids.cpp
  CartesianPoint.cpp
  CartesianTransformationOperator.cpp
  CartesianTransformationOperator2D.cpp
  CartesianTransformationOperator3D.cpp
  CharacterizedDefinition.cpp
  CharacterizedProductDefinition.cpp
  Circle.cpp
  ClosedShell.cpp
  CompositeCurve.cpp
  CompositeCurveOnSurface.cpp
  CompositeCurveSegment.cpp
  CompoundRepresentationItem.cpp
  Conic.cpp
  ConicalSurface.cpp
  ConnectedFaceSet.cpp
  ContextDependentShapeRepresentation.cpp
  ContextDependentUnit.cpp
  ConversionBasedUnit.cpp
  Curve.cpp
  CurveBoundedSurface.cpp
  CurveReplica.cpp
  CsgShapeRepresentation.cpp
  CsgSolid.cpp
  CylindricalSurface.cpp
  DefinitionalRepresentation.cpp
  DerivedUnit.cpp
  DerivedUnitElement.cpp
  DesignContext.cpp
  DimensionalExponents.cpp
  Direction.cpp
  Edge.cpp
  EdgeCurve.cpp
  EdgeLoop.cpp
  ElectricCurrentContextDependentUnit.cpp
  ElectricCurrentConversionBasedUnit.cpp
  ElectricCurrentSiUnit.cpp
  ElectricCurrentUnit.cpp
  ElementarySurface.cpp
  Ellipse.cpp
  ExtrudedAreaSolid.cpp
  Face.cpp
  FaceBound.cpp
  FaceOuterBound.cpp
  FaceSurface.cpp
  FacetedBrep.cpp
  FacetedBrepShapeRepresentation.cpp
  PolyLoop.cpp
  Factory.cpp
  FoundedItem.cpp
  FunctionallyDefinedTransformation.cpp
  GeometricallyBoundedSurfaceShapeRepresentation.cpp
  GeometricallyBoundedWireframeShapeRepresentation.cpp
  GeometricRepresentationContext.cpp
  GeometricRepresentationItem.cpp
  GeometricSet.cpp
  GeometricSetSelect.cpp
  GlobalUncertaintyAssignedContext.cpp
  GlobalUnitAssignedContext.cpp
  Hyperbola.cpp
  IntersectionCurve.cpp
  ItemDefinedTransformation.cpp
  LengthContextDependentUnit.cpp
  LengthConversionBasedUnit.cpp
  LengthMeasureWithUnit.cpp
  LengthSiUnit.cpp
  LengthUnit.cpp
  Line.cpp
  LocalUnits.cpp
  Loop.cpp
  LuminousIntensityContextDependentUnit.cpp
  LuminousIntensityConversionBasedUnit.cpp
  LuminousIntensitySiUnit.cpp
  LuminousIntensityUnit.cpp
  ManifoldSolidBrep.cpp
  ManifoldSurfaceShapeRepresentation.cpp
  MappedItem.cpp
  MassContextDependentUnit.cpp
  MassConversionBasedUnit.cpp
  MassSiUnit.cpp
  MassUnit.cpp
  MeasureValue.cpp
  MeasureWithUnit.cpp
  MechanicalContext.cpp
  NamedUnit.cpp
  NextAssemblyUsageOccurrence.cpp
  OffsetCurve2D.cpp
  OffsetCurve3D.cpp
  OffsetSurface.cpp
  OpenNurbsInterfaces.cpp
  OpenNurbsPrimitives.cpp
  OpenNurbsSplines.cpp
  OpenShell.cpp
  OrientedClosedShell.cpp
  OrientedEdge.cpp
  OrientedFace.cpp
  PCurve.cpp
  PCurveOrSurface.cpp
  Parabola.cpp
  ParametricRepresentationContext.cpp
  Path.cpp
  Placement.cpp
  Plane.cpp
  PlaneAngleContextDependentUnit.cpp
  PlaneAngleConversionBasedUnit.cpp
  PlaneAngleMeasureWithUnit.cpp
  PlaneAngleSiUnit.cpp
  PlaneAngleUnit.cpp
  Point.cpp
  Polyline.cpp
  Product.cpp
  ProductCategory.cpp
  ProductContext.cpp
  ProductDefinition.cpp
  ProductDefinitionContext.cpp
  ProductDefinitionContextAssociation.cpp
  ProductDefinitionContextRole.cpp
  ProductDefinitionFormation.cpp
  ProductDefinitionFormationWithSpecifiedSource.cpp
  ProductDefinitionRelationship.cpp
  ProductDefinitionShape.cpp
  ProductDefinitionUsage.cpp
  ProductDefinitionWithAssociatedDocuments.cpp
  ProductRelatedProductCategory.cpp
  PropertyDefinition.cpp
  PropertyDefinitionRepresentation.cpp
  QuasiUniformCurve.cpp
  QuasiUniformSurface.cpp
  RatioContextDependentUnit.cpp
  RatioConversionBasedUnit.cpp
  RatioSiUnit.cpp
  RatioUnit.cpp
  RationalBSplineCurve.cpp
  RationalBSplineCurveWithKnots.cpp
  RationalBSplineSurface.cpp
  RationalBSplineSurfaceWithKnots.cpp
  RationalBezierCurve.cpp
  RationalBezierSurface.cpp
  RationalQuasiUniformCurve.cpp
  RationalQuasiUniformSurface.cpp
  RationalUniformCurve.cpp
  RationalUniformSurface.cpp
  RectangularCompositeSurface.cpp
  RectangularTrimmedSurface.cpp
  Representation.cpp
  RepresentationContext.cpp
  RepresentationItem.cpp
  RepresentationMap.cpp
  RepresentationRelationship.cpp
  RepresentationRelationshipWithTransformation.cpp
  RepresentedDefinition.cpp
  RevolvedAreaSolid.cpp
  STEPEntity.cpp
  SeamCurve.cpp
  ShapeAspect.cpp
  ShapeAspectRelationship.cpp
  ShapeDefinition.cpp
  ShapeDefinitionRepresentation.cpp
  ShapeRepresentation.cpp
  ShapeRepresentationRelationship.cpp
  ShellBasedSurfaceModel.cpp
  SiUnit.cpp
  SolidAngleContextDependentUnit.cpp
  SolidAngleConversionBasedUnit.cpp
  SolidAngleSiUnit.cpp
  SolidAngleUnit.cpp
  SolidModel.cpp
  SolidReplica.cpp
  SphericalSurface.cpp
  Surface.cpp
  SurfaceCurve.cpp
  SurfaceOfLinearExtrusion.cpp
  SurfaceOfRevolution.cpp
  SurfacePatch.cpp
  SurfaceReplica.cpp
  SweptAreaSolid.cpp
  SweptSurface.cpp
  ThermodynamicTemperatureContextDependentUnit.cpp
  ThermodynamicTemperatureConversionBasedUnit.cpp
  ThermodynamicTemperatureSiUnit.cpp
  ThermodynamicTemperatureUnit.cpp
  TimeContextDependentUnit.cpp
  TimeConversionBasedUnit.cpp
  TimeSiUnit.cpp
  TimeUnit.cpp
  TopologicalRepresentationItem.cpp
  ToroidalSurface.cpp
  Transformation.cpp
  TrimmedCurve.cpp
  TrimmingSelect.cpp
  UncertaintyMeasureWithUnit.cpp
  UniformCurve.cpp
  UniformSurface.cpp
  Unit.cpp
  Vector.cpp
  Vertex.cpp
  VertexLoop.cpp
  VertexPoint.cpp
  VolumeContextDependentUnit.cpp
  VolumeConversionBasedUnit.cpp
  VolumeSiUnit.cpp
  VolumeUnit.cpp
  step-g.cpp
)

if(STEPCODE_LAZY_FOUND)
  list(APPEND stepg_SOURCES ../STEPLazySession.cpp)
endif()
cmakefiles(${stepg_SOURCES})

set(
  stepg_HDRS
  AdvancedBrepShapeRepresentation.h
  AdvancedFace.h
  AmountOfSubstanceContextDependentUnit.h
  AmountOfSubstanceConversionBasedUnit.h
  AmountOfSubstanceSiUnit.h
  AmountOfSubstanceUnit.h
  ApplicationContext.h
  ApplicationContextElement.h
  AreaContextDependentUnit.h
  AreaConversionBasedUnit.h
  AreaSiUnit.h
  AreaUnit.h
  AssemblyComponentUsage.h
  Axis1Placement.h
  Axis2Placement.h
  Axis2Placement2D.h
  Axis2Placement3D.h
  BSplineCurve.h
  BSplineCurveWithKnots.h
  BSplineSurface.h
  BSplineSurfaceWithKnots.h
  BezierCurve.h
  BezierSurface.h
  BoundaryCurve.h
  BoundedCurve.h
  BoundedPCurve.h
  BoundedSurface.h
  BoundedSurfaceCurve.h
  BrepWithVoids.h
  CartesianPoint.h
  CartesianTransformationOperator.h
  CartesianTransformationOperator2D.h
  CartesianTransformationOperator3D.h
  CharacterizedDefinition.h
  CharacterizedProductDefinition.h
  Circle.h
  ClosedShell.h
  CompositeCurve.h
  CompositeCurveOnSurface.h
  CompositeCurveSegment.h
  CompoundRepresentationItem.h
  Conic.h
  ConicalSurface.h
  ConnectedFaceSet.h
  ContextDependentShapeRepresentation.h
  ContextDependentUnit.h
  ConversionBasedUnit.h
  Curve.h
  CurveBoundedSurface.h
  CurveReplica.h
  CylindricalSurface.h
  DefinitionalRepresentation.h
  DerivedUnit.h
  DerivedUnitElement.h
  DesignContext.h
  DimensionalExponents.h
  Direction.h
  Edge.h
  EdgeCurve.h
  EdgeLoop.h
  ElectricCurrentContextDependentUnit.h
  ElectricCurrentConversionBasedUnit.h
  ElectricCurrentSiUnit.h
  ElectricCurrentUnit.h
  ElementarySurface.h
  Ellipse.h
  ExtrudedAreaSolid.h
  Face.h
  FaceBound.h
  FaceOuterBound.h
  FaceSurface.h
  FacetedBrep.h
  FacetedBrepShapeRepresentation.h
  PolyLoop.h
  Factory.h
  FoundedItem.h
  FunctionallyDefinedTransformation.h
  GeometricallyBoundedSurfaceShapeRepresentation.h
  GeometricallyBoundedWireframeShapeRepresentation.h
  GeometricRepresentationContext.h
  GeometricRepresentationItem.h
  GeometricSet.h
  GeometricSetSelect.h
  GlobalUncertaintyAssignedContext.h
  GlobalUnitAssignedContext.h
  Hyperbola.h
  IntersectionCurve.h
  ItemDefinedTransformation.h
  LengthContextDependentUnit.h
  LengthConversionBasedUnit.h
  LengthMeasureWithUnit.h
  LengthSiUnit.h
  LengthUnit.h
  Line.h
  LocalUnits.h
  Loop.h
  LuminousIntensityContextDependentUnit.h
  LuminousIntensityConversionBasedUnit.h
  LuminousIntensitySiUnit.h
  LuminousIntensityUnit.h
  ManifoldSolidBrep.h
  ManifoldSurfaceShapeRepresentation.h
  MappedItem.h
  MassContextDependentUnit.h
  MassConversionBasedUnit.h
  MassSiUnit.h
  MassUnit.h
  MeasureValue.h
  MeasureWithUnit.h
  MechanicalContext.h
  NamedUnit.h
  NextAssemblyUsageOccurrence.h
  OffsetCurve2D.h
  OffsetCurve3D.h
  OffsetSurface.h
  OpenShell.h
  OrientedClosedShell.h
  OrientedEdge.h
  OrientedFace.h
  PCurve.h
  PCurveOrSurface.h
  Parabola.h
  ParametricRepresentationContext.h
  Path.h
  Placement.h
  Plane.h
  PlaneAngleContextDependentUnit.h
  PlaneAngleConversionBasedUnit.h
  PlaneAngleMeasureWithUnit.h
  PlaneAngleSiUnit.h
  PlaneAngleUnit.h
  Point.h
  Polyline.h
  Product.h
  ProductCategory.h
  ProductContext.h
  ProductDefinition.h
  ProductDefinitionContext.h
  ProductDefinitionContextAssociation.h
  ProductDefinitionContextRole.h
  ProductDefinitionFormation.h
  ProductDefinitionFormationWithSpecifiedSource.h
  ProductDefinitionRelationship.h
  ProductDefinitionShape.h
  ProductDefinitionUsage.h
  ProductDefinitionWithAssociatedDocuments.h
  ProductRelatedProductCategory.h
  PropertyDefinition.h
  PropertyDefinitionRepresentation.h
  QuasiUniformCurve.h
  QuasiUniformSurface.h
  RatioContextDependentUnit.h
  RatioConversionBasedUnit.h
  RatioSiUnit.h
  RatioUnit.h
  RationalBSplineCurve.h
  RationalBSplineCurveWithKnots.h
  RationalBSplineSurface.h
  RationalBSplineSurfaceWithKnots.h
  RationalBezierCurve.h
  RationalBezierSurface.h
  RationalQuasiUniformCurve.h
  RationalQuasiUniformSurface.h
  RationalUniformCurve.h
  RationalUniformSurface.h
  RectangularCompositeSurface.h
  RectangularTrimmedSurface.h
  Representation.h
  RepresentationContext.h
  RepresentationItem.h
  RepresentationMap.h
  RepresentationRelationship.h
  RepresentationRelationshipWithTransformation.h
  RepresentedDefinition.h
  RevolvedAreaSolid.h
  STEPEntity.h
  SeamCurve.h
  ShapeAspect.h
  ShapeAspectRelationship.h
  ShapeDefinition.h
  ShapeDefinitionRepresentation.h
  ShapeRepresentation.h
  ShapeRepresentationRelationship.h
  ShellBasedSurfaceModel.h
  SiUnit.h
  SolidAngleContextDependentUnit.h
  SolidAngleConversionBasedUnit.h
  SolidAngleSiUnit.h
  SolidAngleUnit.h
  SolidModel.h
  SolidReplica.h
  SphericalSurface.h
  Surface.h
  SurfaceCurve.h
  SurfaceOfLinearExtrusion.h
  SurfaceOfRevolution.h
  SurfacePatch.h
  SurfaceReplica.h
  SweptAreaSolid.h
  SweptSurface.h
  ThermodynamicTemperatureContextDependentUnit.h
  ThermodynamicTemperatureConversionBasedUnit.h
  ThermodynamicTemperatureSiUnit.h
  ThermodynamicTemperatureUnit.h
  TimeContextDependentUnit.h
  TimeConversionBasedUnit.h
  TimeSiUnit.h
  TimeUnit.h
  TopologicalRepresentationItem.h
  ToroidalSurface.h
  Transformation.h
  TrimmedCurve.h
  TrimmingSelect.h
  UncertaintyMeasureWithUnit.h
  UniformCurve.h
  UniformSurface.h
  Unit.h
  Vector.h
  Vertex.h
  VertexLoop.h
  VertexPoint.h
  VolumeContextDependentUnit.h
  VolumeConversionBasedUnit.h
  VolumeSiUnit.h
  VolumeUnit.h
)
cmakefiles(${stepg_HDRS})

if(BRLCAD_ENABLE_STEP)
  set(
    stepg_LIBS
    libwdb
    librt
    libbu
    libbrep
    ${stepg_SCHEMA_BINDINGS_TARGET}
    ${STEPCODE_UTILS_LIBRARY}
    ${STEPCODE_EDITOR_LIBRARY}
    ${STEPCODE_DAI_LIBRARY}
    ${STEPCODE_CORE_LIBRARY}
    OPENNURBS::OPENNURBS
  )

  if(STEPCODE_LAZY_FOUND)
    list(APPEND stepg_LIBS ${STEPCODE_LAZY_LIBRARY})
  endif()

  set(
    AP203_INCLUDE_DIRS
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${STEPCODE_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src/libbrep # for PullbackCurve.h
  )

  # We have the PRECOMILE_HEADER that includes the generated stepcode
  # outputs - add our fixed headers as well:
  foreach(header ${stepg_HDRS})
    # Use relative include for portability
    get_filename_component(header_name "${header}" NAME)
    file(APPEND "${stepg_PRECOMPILE_HEADER}" "#include \"${header_name}\"\n")
  endforeach()

  step_use_import_common_objects(stepg_TARGET_SOURCES stepg_SOURCES)
  step_record_schema_sources(AP203 IMPORT stepg_TARGET_SOURCES)
  brlcad_addexec(step-g "../StepImportHost.cpp" "libbu" NO_STRICT FOLDER Conv
    # Entity source files each define CLASSNAME and ENTITYNAME macros.  Undef
    # them before each file's include so they don't collide when the unity
    # build batches multiple entity files into one compilation unit.
    UNITY_BUILD_CODE_BEFORE_INCLUDE "#undef CLASSNAME\n#undef ENTITYNAME"
  )

  if(ENABLE_ALL_CXX_COMPILE)
    target_compile_definitions(step-g PRIVATE register=)
  endif()
  target_include_directories(step-g BEFORE PRIVATE ${AP203_INCLUDE_DIRS})
  if(STEPCODE_LAZY_FOUND)
    brlcad_add_test(
      NAME ap203_lazy_positional
      COMMAND
        ${CMAKE_COMMAND}
        -DSTEP_G=$<TARGET_FILE:step-g>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_direct_manifold.stp
        -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_direct_manifold.g
        -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_budget_controls.json
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_lazy_positional.cmake
    )
    brlcad_add_test(
      NAME ap203_multi_schema_header
      COMMAND
        ${CMAKE_COMMAND}
        -DSTEP_G=$<TARGET_FILE:step-g>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_direct_manifold.stp
        -DMULTI_INPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_multi_schema_header.stp
        -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_multi_schema_header.g
        -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_multi_schema_header.json
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_multi_schema_header.cmake
    )
    brlcad_add_test(
      NAME ap203_lazy_relationships
      COMMAND
        ${CMAKE_COMMAND}
        -DSTEP_G=$<TARGET_FILE:step-g>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_direct_manifold.stp
        -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_lazy_relationships.json
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_lazy_relationships.cmake
    )
    brlcad_add_test(
      NAME ap203_representation_coverage
      COMMAND
        ${CMAKE_COMMAND}
        -DSTEP_G=$<TARGET_FILE:step-g>
        -DUNSUPPORTED_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_unsupported_geometry.stp
        -DMALFORMED_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_malformed_representation.stp
        -DEMPTY_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_non_geometric_representation.stp
        -DPARTIAL_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_direct_manifold.stp
        -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_representation_coverage.cmake
    )
    brlcad_add_test(
      NAME ap203_periodic_zero_split
      COMMAND
        ${CMAKE_COMMAND}
        -DSTEP_G=$<TARGET_FILE:step-g>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_periodic_zero_split.stp
        -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_periodic_zero_split.json
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_periodic_zero_split.cmake
    )
    if (TARGET mged)
      brlcad_add_test(
	NAME ap203_near_integer_spherical_winding
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_near_integer_spherical_winding.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_near_integer_spherical_winding.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_near_integer_spherical_winding.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_near_integer_spherical_winding.cmake
      )
      brlcad_add_test(
	NAME ap203_shifted_four_arc_spherical_cap
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_shifted_four_arc_spherical_cap.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_shifted_four_arc_spherical_cap.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_shifted_four_arc_spherical_cap.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_shifted_four_arc_spherical_cap.cmake
      )
      brlcad_add_test(
	NAME ap203_single_singular_periodic_branch
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_single_singular_periodic_branch.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_single_singular_periodic_branch.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_single_singular_periodic_branch.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_single_singular_periodic_branch.cmake
      )
      brlcad_add_test(
	NAME ap203_false_face_closed_edge_use
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_false_face_closed_edge_use.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_false_face_closed_edge_use.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_false_face_closed_edge_use.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_false_face_closed_edge_use.cmake
      )
    endif()
    brlcad_add_test(
      NAME ap203_doubly_periodic_native_remap_transaction
      COMMAND
        ${CMAKE_COMMAND}
        -DSTEP_G=$<TARGET_FILE:step-g>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_doubly_periodic_native_remap_transaction.stp
        -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_doubly_periodic_native_remap_transaction.json
        -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_doubly_periodic_native_remap_transaction.g
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_doubly_periodic_native_remap_transaction.cmake
    )
    brlcad_add_test(
      NAME ap203_toroidal_band
      COMMAND
        ${CMAKE_COMMAND}
        -DSTEP_G=$<TARGET_FILE:step-g>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_toroidal_band.stp
        -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_toroidal_band.json
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_toroidal_band.cmake
    )
    if (TARGET mged)
      brlcad_add_test(
	NAME ap203_toroidal_band_profile_seam
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_toroidal_band_profile_seam.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_toroidal_band_profile_seam.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_toroidal_band_profile_seam.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_toroidal_band_profile_seam.cmake
      )
      brlcad_add_test(
	NAME ap203_toroidal_band_topology_seam
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_toroidal_band_topology_seam.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_toroidal_band_topology_seam.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_toroidal_band_topology_seam.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_toroidal_band_topology_seam.cmake
      )
      brlcad_add_test(
	NAME ap203_measured_periodic_band_seam
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_measured_periodic_band_seam.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_measured_periodic_band_seam.json
	-DEXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_measured_periodic_band_seam_exact.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_measured_periodic_band_seam.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_measured_periodic_band_seam.cmake
      )
      brlcad_add_test(
	NAME ap203_doubly_periodic_segmented_band
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_doubly_periodic_segmented_band.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_doubly_periodic_segmented_band.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_doubly_periodic_segmented_band.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_doubly_periodic_segmented_band.cmake
      )
      brlcad_add_test(
	NAME ap203_shared_periodic_boundary_split
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_shared_periodic_boundary_split.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_shared_periodic_boundary_split.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_shared_periodic_boundary_split.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_shared_periodic_boundary_split.cmake
      )
      brlcad_add_test(
	NAME ap203_singly_periodic_isocurve_order
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_singly_periodic_isocurve_order.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_singly_periodic_isocurve_order.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_singly_periodic_isocurve_order.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_singly_periodic_isocurve_order.cmake
      )
    endif()
    # These roots are distinct scale/tolerance instances of the same
    # periodic-pole branch join.
    brlcad_add_test(
      NAME ap203_scaled_periodic_pole_solids
      COMMAND
        ${CMAKE_COMMAND}
        -DSTEP_G=$<TARGET_FILE:step-g>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_scaled_periodic_pole_solids.stp
        -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_scaled_periodic_pole_solids.json
        -DENTITY_IDS=92712,122567
        -DEXPECTED_ATTEMPTED=2
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_selected_solid_regression.cmake
    )
    # This root represents the same periodic-pole failure as a larger sibling
    # while retaining a compact and independently traceable solid.
    brlcad_add_test(
      NAME ap203_periodic_pole_solid
      COMMAND
        ${CMAKE_COMMAND}
        -DSTEP_G=$<TARGET_FILE:step-g>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_periodic_pole_solid.stp
        -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_periodic_pole_solid.json
        -DENTITY_IDS=4716
        -DEXPECTED_ATTEMPTED=1
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_selected_solid_regression.cmake
    )
    # These two exact roots guard the exhaustive full-period cut/winding
    # search that previously consumed the complete per-item budget.
    brlcad_add_test(
      NAME ap203_periodic_budget_solid_a
      COMMAND
        ${CMAKE_COMMAND}
        -DSTEP_G=$<TARGET_FILE:step-g>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_periodic_budget_solid_a.stp
        -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_periodic_budget_solid_a.json
        -DENTITY_IDS=124432
        -DEXPECTED_ATTEMPTED=1
        -DTEST_TIMEOUT=30
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_selected_solid_regression.cmake
    )
    brlcad_add_test(
      NAME ap203_periodic_budget_solid_b
      COMMAND
        ${CMAKE_COMMAND}
        -DSTEP_G=$<TARGET_FILE:step-g>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_periodic_budget_solid_b.stp
        -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_periodic_budget_solid_b.json
        -DENTITY_IDS=4524
        -DEXPECTED_ATTEMPTED=1
        -DTEST_TIMEOUT=30
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_selected_solid_regression.cmake
    )
    if (TARGET mged)
      brlcad_add_test(
	NAME ap203_torus_seam_relocation
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_torus_seam_relocation.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_torus_seam_relocation.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_torus_seam_relocation.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_torus_seam_relocation.cmake
      )
      brlcad_add_test(
	NAME ap203_extruded_closed_profile_seam
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_extruded_closed_profile_seam.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_extruded_closed_profile_seam.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_extruded_closed_profile_seam.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_extruded_closed_profile_seam.cmake
      )
      brlcad_add_test(
	NAME ap203_near_degenerate_nurbs_pole
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_near_degenerate_nurbs_pole.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_near_degenerate_nurbs_pole.json
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_near_degenerate_nurbs_pole.cmake
      )
      brlcad_add_test(
	NAME ap203_periodic_bspline_triangular_cap
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_periodic_bspline_triangular_cap.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_periodic_bspline_triangular_cap.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_periodic_bspline_triangular_cap.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_periodic_bspline_triangular_cap.cmake
      )
      brlcad_add_test(
	NAME ap203_conical_multi_edge_periodic_cap
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_conical_multi_edge_periodic_cap.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_conical_multi_edge_periodic_cap.json
	-DEXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_conical_multi_edge_periodic_cap_exact.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_conical_multi_edge_periodic_cap.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_conical_multi_edge_periodic_cap.cmake
      )
    endif()
    brlcad_add_test(
      NAME ap203_closed_nurbs_interval
      COMMAND
        ${CMAKE_COMMAND}
        -DSTEP_G=$<TARGET_FILE:step-g>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_closed_nurbs_interval.stp
        -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_closed_nurbs_interval.json
        -DEXACT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_closed_nurbs_interval_exact.json
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_closed_nurbs_interval.cmake
    )
    brlcad_add_test(
      NAME ap203_closed_curve_seam_interval
      COMMAND
        ${CMAKE_COMMAND}
        -DSTEP_G=$<TARGET_FILE:step-g>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_closed_curve_seam_interval.stp
        -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_closed_curve_seam_interval.json
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_closed_curve_seam_interval.cmake
    )
    if (TARGET mged)
      brlcad_add_test(
	NAME ap203_unclamped_closed_curve_knots
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_unclamped_closed_curve_knots.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_unclamped_closed_curve_knots.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_unclamped_closed_curve_knots.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_unclamped_closed_curve_knots.cmake
      )
      brlcad_add_test(
	NAME ap203_bounded_parent_curve
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_bounded_parent_curve.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_bounded_parent_curve.json
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_bounded_parent_curve.cmake
      )
      brlcad_add_test(
	NAME ap203_wip_compat
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DFACETED_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_faceted_tetra.stp
	-DMAPPED_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_mapped_assembly.stp
	-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_wip_compat.cmake
      )
      brlcad_add_test(
	NAME ap203_invalid_brep
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_invalid_brep.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_invalid_brep.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_invalid_brep.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_invalid_brep.cmake
      )
      brlcad_add_test(
	NAME ap203_bezier_offset_wireframe
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_bezier_offset_wireframe.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_bezier_offset_wireframe.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_bezier_offset_wireframe.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_bezier_offset_wireframe.cmake
      )
      brlcad_add_test(
	NAME ap203_part21_scope
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_part21_scope.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_part21_scope.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_part21_scope.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_part21_scope.cmake
      )
      brlcad_add_test(
	NAME ap203_recommended_hierarchy
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_referenced_two_level.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_recommended_hierarchy.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_recommended_hierarchy.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_recommended_hierarchy.cmake
      )
      brlcad_add_test(
	NAME ap203_usage_semantics
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DG_STEP=$<TARGET_FILE:g-step>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_usage_semantics.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_usage_semantics.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_usage_semantics.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_usage_semantics.cmake
      )
      brlcad_add_test(
	NAME ap203_topology_wireframes
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_topology_wireframes.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_topology_wireframes.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_topology_wireframes.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_topology_wireframes.cmake
      )
      brlcad_add_test(
	NAME ap203_appendix_j_multiple_sdr
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_appendix_j_multiple_sdr.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_appendix_j_multiple_sdr.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_appendix_j_multiple_sdr.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_appendix_j_multiple_sdr.cmake
      )
      brlcad_add_test(
	NAME ap203_bounded_surface_curves
	COMMAND
	${CMAKE_COMMAND}
	-DSTEP_G=$<TARGET_FILE:step-g>
	-DMGED=$<TARGET_FILE:mged>
	-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_bounded_surface_curves.stp
	-DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203_bounded_surface_curves.json
	-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203_bounded_surface_curves.g
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203_bounded_surface_curves.cmake
      )
    endif()
  endif()

  if(HIDE_INTERNAL_SYMBOLS)
    set(
      SCIMPORTS
      SC_CORE_DLL_IMPORTS
      SC_DAI_DLL_IMPORTS
      SC_EDITOR_DLL_IMPORTS
      SC_UTILS_DLL_IMPORTS
    )
    foreach(scp ${SCIMPORTS})
      set_property(TARGET step-g APPEND PROPERTY COMPILE_DEFINITIONS "${scp}")
    endforeach(scp ${SCIMPORTS})
  endif(HIDE_INTERNAL_SYMBOLS)

  # On some platforms distclean has additional cleanup to do
  distclean("${CMAKE_BINARY_DIR}/${LIB_DIR}/step-g.exp") 
  distclean("${CMAKE_BINARY_DIR}/${LIB_DIR}/step-g.lib") 

endif(BRLCAD_ENABLE_STEP)

cmakefiles(
  CMakeLists.txt
  CsgShapeRepresentation.h
  CsgSolid.h
  OpenNurbsInterfaces.h
  OpenNurbsPullback.inc
  OpenNurbsTopology.inc
  tests/ap203_appendix_j_multiple_sdr.cmake
  tests/ap203_appendix_j_multiple_sdr.stp
  tests/ap203_bezier_offset_wireframe.cmake
  tests/ap203_bezier_offset_wireframe.stp
  tests/ap203_bounded_surface_curves.cmake
  tests/ap203_bounded_surface_curves.stp
  tests/ap203_part21_scope.cmake
  tests/ap203_part21_scope.stp
  tests/ap203_bounded_parent_curve.cmake
  tests/ap203_bounded_parent_curve.stp
  tests/ap203_faceted_tetra.stp
  tests/ap203_invalid_brep.cmake
  tests/ap203_invalid_brep.stp
  tests/ap203_mapped_assembly.stp
  tests/ap203_recommended_hierarchy.cmake
  tests/ap203_referenced_two_level.stp
  tests/ap203_topology_wireframes.cmake
  tests/ap203_topology_wireframes.stp
  tests/ap203_usage_semantics.cmake
  tests/ap203_usage_semantics.stp
  tests/ap203_wip_compat.cmake
  ap203.exp
  tests/ap203_closed_curve_seam_interval.cmake
  tests/ap203_closed_curve_seam_interval.stp
  tests/ap203_closed_nurbs_interval.cmake
  tests/ap203_closed_nurbs_interval.stp
  tests/ap203_conical_multi_edge_periodic_cap.cmake
  tests/ap203_conical_multi_edge_periodic_cap.stp
  tests/ap203_selected_solid_regression.cmake
  tests/ap203_direct_manifold.stp
  tests/ap203_doubly_periodic_native_remap_transaction.cmake
  tests/ap203_doubly_periodic_native_remap_transaction.stp
  tests/ap203_doubly_periodic_segmented_band.cmake
  tests/ap203_doubly_periodic_segmented_band.stp
  tests/ap203_extruded_closed_profile_seam.cmake
  tests/ap203_extruded_closed_profile_seam.stp
  tests/ap203_false_face_closed_edge_use.cmake
  tests/ap203_false_face_closed_edge_use.stp
  tests/ap203_lazy_positional.cmake
  tests/ap203_lazy_relationships.cmake
  tests/ap203_multi_schema_header.cmake
  tests/ap203_measured_periodic_band_seam.cmake
  tests/ap203_measured_periodic_band_seam.stp
  tests/ap203_periodic_budget_solid_a.stp
  tests/ap203_scaled_periodic_pole_solids.stp
  tests/ap203_periodic_budget_solid_b.stp
  tests/ap203_periodic_pole_solid.stp
  tests/ap203_near_degenerate_nurbs_pole.cmake
  tests/ap203_near_degenerate_nurbs_pole.stp
  tests/ap203_near_integer_spherical_winding.cmake
  tests/ap203_near_integer_spherical_winding.stp
  tests/ap203_periodic_bspline_triangular_cap.cmake
  tests/ap203_periodic_bspline_triangular_cap.stp
  tests/ap203_periodic_zero_split.cmake
  tests/ap203_periodic_zero_split.stp
  tests/ap203_malformed_representation.stp
  tests/ap203_non_geometric_representation.stp
  tests/ap203_representation_coverage.cmake
  tests/ap203_unsupported_geometry.stp
  tests/ap203_shared_periodic_boundary_split.cmake
  tests/ap203_shared_periodic_boundary_split.stp
  tests/ap203_single_singular_periodic_branch.cmake
  tests/ap203_single_singular_periodic_branch.stp
  tests/ap203_singly_periodic_isocurve_order.cmake
  tests/ap203_singly_periodic_isocurve_order.stp
  tests/ap203_shifted_four_arc_spherical_cap.cmake
  tests/ap203_shifted_four_arc_spherical_cap.stp
  tests/ap203_toroidal_band.cmake
  tests/ap203_toroidal_band.stp
  tests/ap203_toroidal_band_profile_seam.cmake
  tests/ap203_toroidal_band_profile_seam.stp
  tests/ap203_toroidal_band_topology_seam.cmake
  tests/ap203_toroidal_band_topology_seam.stp
  tests/ap203_torus_seam_relocation.cmake
  tests/ap203_torus_seam_relocation.stp
  tests/ap203_unclamped_closed_curve_knots.cmake
  tests/ap203_unclamped_closed_curve_knots.stp
)

set(_ap203_generated_tests
  budget_controls
  closed_curve_seam_interval
  closed_nurbs_interval
  closed_nurbs_interval_exact
  direct_manifold
  doubly_periodic_native_remap_transaction
  lazy_relationships
  malformed_representation
  multi_schema_header
  non_geometric_representation
  partial_coverage
  partial_coverage_strict
  periodic_budget_solid_a
  periodic_budget_solid_b
  periodic_pole_solid
  periodic_zero_split
  scaled_periodic_pole_solids
  toroidal_band
  unsupported_geometry
  unsupported_geometry_strict
)
foreach(_test_name IN LISTS _ap203_generated_tests)
  foreach(_extension IN ITEMS json g stp csv)
    distclean("${CMAKE_CURRENT_BINARY_DIR}/ap203_${_test_name}.${_extension}")
  endforeach()
endforeach()
distclean("${CMAKE_CURRENT_BINARY_DIR}/ap203_toroidal_band.json.repair-none")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
