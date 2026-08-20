# Schema-neutral entity wrappers shared by AP203, AP203e2, AP214 and AP242.
# Keep this list explicit: adding a source here is an architectural assertion
# that it neither includes generated bindings nor changes with an AP macro.
set(
  STEP_IMPORT_COMMON_BASENAMES
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
  Axis1Placement.cpp
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
  CsgShapeRepresentation.cpp
  CsgSolid.cpp
  Curve.cpp
  CurveBoundedSurface.cpp
  CurveReplica.cpp
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
  Factory.cpp
  FoundedItem.cpp
  FunctionallyDefinedTransformation.cpp
  GeometricRepresentationContext.cpp
  GeometricRepresentationItem.cpp
  GeometricSet.cpp
  GeometricallyBoundedSurfaceShapeRepresentation.cpp
  GeometricallyBoundedWireframeShapeRepresentation.cpp
  GlobalUncertaintyAssignedContext.cpp
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
  MechanicalContext.cpp
  NamedUnit.cpp
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
  PolyLoop.cpp
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
  ProductDefinitionWithAssociatedDocuments.cpp
  ProductRelatedProductCategory.cpp
  PropertyDefinition.cpp
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
  RevolvedAreaSolid.cpp
  STEPEntity.cpp
  SeamCurve.cpp
  ShapeAspect.cpp
  ShapeAspectRelationship.cpp
  ShapeDefinitionRepresentation.cpp
  ShapeRepresentation.cpp
  ShapeRepresentationRelationship.cpp
  SiUnit.cpp
  SolidAngleContextDependentUnit.cpp
  SolidAngleConversionBasedUnit.cpp
  SolidAngleSiUnit.cpp
  SolidAngleUnit.cpp
  SolidModel.cpp
  SolidReplica.cpp
  SphericalSurface.cpp
  Surface.cpp
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
)

set(STEP_IMPORT_COMMON_SOURCES)
foreach(_basename IN LISTS STEP_IMPORT_COMMON_BASENAMES)
  set(_source "${CMAKE_CURRENT_SOURCE_DIR}/step-g/${_basename}")
  list(APPEND STEP_IMPORT_COMMON_SOURCES "${_source}")

  # Guard both the implementation and its same-named public header.  This
  # catches the two ways schema dependencies historically entered otherwise
  # generic wrappers: generated includes and AP-dependent preprocessing.
  set(_guard_files "${_source}")
  string(REGEX REPLACE "\\.cpp$" ".h" _header "${_source}")
  if(EXISTS "${_header}")
    list(APPEND _guard_files "${_header}")
  endif()
  foreach(_guard_file IN LISTS _guard_files)
    file(READ "${_guard_file}" _contents)
    if(_contents MATCHES "ap_schema\\.h|SCHEMA_NAMESPACE|#[ \t]*(if|ifdef|ifndef)[^\n]*AP(203|214|242)|#[ \t]*include[ \t]*[<\"][^>\"]*Sdai")
      message(FATAL_ERROR
        "Schema-dependent source ${_guard_file} is listed in step_import_common_objects"
      )
    endif()
  endforeach()
endforeach()

# Schema-neutral support sources live one directory above the entity wrappers.
# Keep them separate from STEP_IMPORT_COMMON_BASENAMES because that list is
# deliberately resolved relative to step-g/.
set(
  STEP_IMPORT_COMMON_ROOT_BASENAMES
  STEPBudget.cpp
  STEPBrepFinalize.cpp
  STEPBrepLoopRepair.cpp
  STEPBrepPeriodic.cpp
  STEPBrepPullback.cpp
  STEPBrepSeamRepair.cpp
  STEPBrepTopology.cpp
  STEPBrepValidation.cpp
  STEPGeometricSet.cpp
  STEPImportPipeline.cpp
  STEPPresentation.cpp
  STEPReport.cpp
  STEPSweptSolid.cpp
  STEPTessellatedMesh.cpp
  STEPWrapper.cpp
  STEPWrapperAttributes.cpp
  STEPWrapperIO.cpp
)
foreach(_basename IN LISTS STEP_IMPORT_COMMON_ROOT_BASENAMES)
  set(_source "${CMAKE_CURRENT_SOURCE_DIR}/${_basename}")
  list(APPEND STEP_IMPORT_COMMON_SOURCES "${_source}")
  file(READ "${_source}" _contents)
  if(_contents MATCHES "ap_schema\\.h|SCHEMA_NAMESPACE|#[ \t]*(if|ifdef|ifndef)[^\n]*AP(203|214|242)|#[ \t]*include[ \t]*[<\"][^>\"]*Sdai")
    message(FATAL_ERROR
      "Schema-dependent source ${_source} is listed in step_import_common_objects"
    )
  endif()
endforeach()

# Replace shared wrapper translation units in a converter source list with
# direct object consumption.  Direct consumption is required so file-scope
# Factory registrations cannot be discarded by an archive linker.
function(STEP_USE_IMPORT_COMMON_OBJECTS output_var input_var)
  set(_result)
  foreach(_source IN LISTS ${input_var})
    get_filename_component(_basename "${_source}" NAME)
    list(FIND STEP_IMPORT_COMMON_BASENAMES "${_basename}" _common_index)
    list(FIND STEP_IMPORT_COMMON_ROOT_BASENAMES "${_basename}" _root_common_index)
    if(_common_index EQUAL -1 AND _root_common_index EQUAL -1)
      list(APPEND _result "${_source}")
    endif()
  endforeach()
  list(APPEND _result $<TARGET_OBJECTS:step_import_common_objects>)
  set(${output_var} "${_result}" PARENT_SCOPE)
endfunction()
