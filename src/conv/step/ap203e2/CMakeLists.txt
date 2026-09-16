set(STEP_SCHEMA_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ap203e2.exp)

if(BRLCAD_ENABLE_STEP)
  generate_schema_inputs(${STEP_SCHEMA_FILE} ap203e2g ap203e2)
  generate_schema_inputs(${STEP_SCHEMA_FILE} gap203e2 ap203e2)
endif(BRLCAD_ENABLE_STEP)

set(
  gap203e2_SOURCES
  ${gap203e2_express_srcs}
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
  ../g-step/G_Objects.cpp
  ../STEPNativeCSGExport.cpp
  ../g-step/ON_NurbsCurve.cpp
  ../g-step/ON_NurbsSurface.cpp
  ../g-step/ON_Brep.cpp
  ../g-step/Shape_Definition_Representation.cpp
  ../g-step/Shape_Representation.cpp
  ../g-step/Shape_Representation_Relationship.cpp
  ../g-step/Trees.cpp
  Export.cpp
)

set(
  ap203e2g_SOURCES
  ${ap203e2g_express_srcs}
  ../BRLCADWrapper.cpp
  ../STEPString.cpp
  ../STEPBudget.cpp
  ../STEPBrepValidation.cpp
  ../STEPWrapper.cpp
  ../STEPWrapperAttributes.cpp
  ../STEPWrapperIO.cpp
  ../STEPNativeCSGImport.cpp
  ../STEPMaterialImport.cpp
  ../step-g/AdvancedBrepShapeRepresentation.cpp
  ../step-g/AdvancedFace.cpp
  ../step-g/AmountOfSubstanceContextDependentUnit.cpp
  ../step-g/AmountOfSubstanceConversionBasedUnit.cpp
  ../step-g/AmountOfSubstanceSiUnit.cpp
  ../step-g/AmountOfSubstanceUnit.cpp
  ../step-g/ApplicationContext.cpp
  ../step-g/ApplicationContextElement.cpp
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
  ../step-g/GeometricSet.cpp
  ../step-g/GeometricSetSelect.cpp
  ../step-g/GeometricallyBoundedSurfaceShapeRepresentation.cpp
  ../step-g/GeometricallyBoundedWireframeShapeRepresentation.cpp
  ../step-g/GeometricRepresentationContext.cpp
  ../step-g/GeometricRepresentationItem.cpp
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
  ../step-g/step-g.cpp
)

set(
  ap203e2g_LIBS
  libwdb
  librt
  libbu
  libbrep
  ${ap203e2g_SCHEMA_BINDINGS_TARGET}
  ${STEPCODE_UTILS_LIBRARY}
  ${STEPCODE_EDITOR_LIBRARY}
  ${STEPCODE_DAI_LIBRARY}
  ${STEPCODE_CORE_LIBRARY}
  OPENNURBS::OPENNURBS
)

if(STEPCODE_LAZY_FOUND)
  list(APPEND ap203e2g_SOURCES ../STEPLazySession.cpp)
  list(APPEND ap203e2g_LIBS ${STEPCODE_LAZY_LIBRARY})
endif()

set(
  AP203_INCLUDE_DIRS
  ${CMAKE_CURRENT_BINARY_DIR}
  ${ap203e2g_SCHEMA_OUT_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../step-g
  ${STEPCODE_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src/libbrep # for PullbackCurve.h
)

if(BRLCAD_ENABLE_STEP)
  step_use_import_common_objects(ap203e2g_TARGET_SOURCES ap203e2g_SOURCES)
  step_record_schema_sources(AP203E2 IMPORT ap203e2g_TARGET_SOURCES)
  step_record_schema_sources(AP203E2 EXPORT gap203e2_SOURCES)

  if (TARGET mged)
    brlcad_add_test(
      NAME ap203e2_csg
      COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203e2_csg.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_csg.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_csg.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203e2_csg.cmake
    )
    brlcad_add_test(
      NAME ap203e2_swept_solids
      COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203e2_swept_solids.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_swept_solids.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_swept_solids.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203e2_swept_solids.cmake
    )
    brlcad_add_test(
      NAME ap203e2_mapped_assembly
      COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap203e2_mapped_assembly.stp
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_mapped_assembly.g
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_mapped_assembly.json
      -DSTRICT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_mapped_assembly_strict.json
      -DTOP=AP203e2_Pair
      -DCHILD=AP203e2_Block
      -DASSOCIATION=SHAPE_DEFINITION_REPRESENTATION
      -DSCHEMA_METADATA=STEP::AP203e2::FILE_SCHEMA
      -DTOLERANCE=0.005
      "-DTRANSFORM_MARKER={1 0 0 35"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_mapped_assembly_baseline.cmake
    )
    brlcad_add_test(
      NAME ap203e2_direct_advanced_brep
      COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DTEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_direct_advanced_brep.stp.in
      -DINPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_direct_advanced_brep.stp
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_direct_advanced_brep.g
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_direct_advanced_brep.json
      -DSCHEMA=AP203_CONFIGURATION_CONTROLLED_3D_DESIGN_OF_MECHANICAL_PARTS_AND_ASSEMBLIES_MIM_LF
      -DSCHEMA_LABEL=AP203e2
      -DFILE_NAME=ap203e2_direct_advanced_brep.stp
      -DSOLID_NAME=AP203e2_direct_tetrahedron
      -DTOLERANCE=0.004
      "-DAPPLICATION=configuration controlled 3D designs of mechanical parts and assemblies"
      -DPRODUCT_ID=ap203e2_direct_tetra
      "-DPRODUCT_NAME=AP203e2 Direct Tetra"
      -DASSOCIATION=SHAPE_DEFINITION_REPRESENTATION
      -DTOP=AP203e2_Direct_Tetra
      -P ${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_direct_advanced_brep.cmake
    )
    brlcad_add_test(
      NAME ap203e2_invalid_brep_policy
      COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DTEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_invalid_surface_brep.stp.in
      -DINPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_invalid_surface_brep.stp
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_invalid_surface_brep.g
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_invalid_surface_brep.json
      -DSCHEMA=AP203_CONFIGURATION_CONTROLLED_3D_DESIGN_OF_MECHANICAL_PARTS_AND_ASSEMBLIES_MIM_LF
      -DSCHEMA_LABEL=AP203e2
      -DFILE_NAME=ap203e2_invalid_surface_brep.stp
      "-DAPPLICATION=configuration controlled 3D designs of mechanical parts and assemblies"
      -DPRODUCT_ID=ap203e2_invalid
      "-DPRODUCT_NAME=AP203e2 Invalid"
      -DASSOCIATION=SHAPE_DEFINITION_REPRESENTATION
      -P ${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_invalid_brep_policy.cmake
    )
    brlcad_add_test(
      NAME ap203e2_policy_outcomes
      COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DUNSUPPORTED_TEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_unsupported_product_geometry.stp.in
      -DMALFORMED_TEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_malformed_product_representation.stp.in
      -DEMPTY_TEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_non_geometric_product.stp.in
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -DSCHEMA=AP203_CONFIGURATION_CONTROLLED_3D_DESIGN_OF_MECHANICAL_PARTS_AND_ASSEMBLIES_MIM_LF
      -DSCHEMA_LABEL=AP203e2
      "-DAPPLICATION=configuration controlled 3D designs of mechanical parts and assemblies"
      -DASSOCIATION=SHAPE_DEFINITION_REPRESENTATION
      -P ${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_schema_policy_outcomes.cmake
    )
    brlcad_add_test(
      NAME ap203e2_partial_policy
      COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DTEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_direct_advanced_brep.stp.in
      -DINPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_partial_policy.stp
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_partial_policy.g
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_partial_policy.json
      -DSTRICT_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_partial_policy_strict.g
      -DSTRICT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_partial_policy_strict.json
      -DSCHEMA=AP203_CONFIGURATION_CONTROLLED_3D_DESIGN_OF_MECHANICAL_PARTS_AND_ASSEMBLIES_MIM_LF
      -DSCHEMA_LABEL=AP203e2
      -DFILE_NAME=ap203e2_partial_policy.stp
      -DSOLID_NAME=AP203e2_partial_tetrahedron
      -DTOLERANCE=0.004
      "-DAPPLICATION=configuration controlled 3D designs of mechanical parts and assemblies"
      -DPRODUCT_ID=ap203e2_partial
      "-DPRODUCT_NAME=AP203e2 Partial"
      -DASSOCIATION=SHAPE_DEFINITION_REPRESENTATION
      -P ${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_partial_policy_outcome.cmake
    )
    brlcad_add_test(
      NAME ap203e2_wireframe
      COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DTEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_wireframe.stp.in
      -DINPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_wireframe.stp
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_wireframe.g
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_wireframe.json
      -DSCHEMA=AP203_CONFIGURATION_CONTROLLED_3D_DESIGN_OF_MECHANICAL_PARTS_AND_ASSEMBLIES_MIM_LF
      -DSCHEMA_LABEL=AP203e2
      -DFILE_NAME=ap203e2_wireframe.stp
      -DTOLERANCE=0.006
      "-DAPPLICATION=configuration controlled 3D designs of mechanical parts and assemblies"
      -DPRODUCT_ID=ap203e2_wire_product
      "-DPRODUCT_NAME=AP203e2 Wire Product"
      -DASSOCIATION=SHAPE_DEFINITION_REPRESENTATION
      -DTOP=AP203e2_Wire_Product
      -P ${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_wireframe_baseline.cmake
    )
    brlcad_add_test(
      NAME ap203e2_surface_models
      COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DTEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_direct_advanced_brep.stp.in
      -DINPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_surface_models.stp
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_surface_models.g
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_surface_models.json
      -DSCHEMA=AP203_CONFIGURATION_CONTROLLED_3D_DESIGN_OF_MECHANICAL_PARTS_AND_ASSEMBLIES_MIM_LF
      -DSCHEMA_LABEL=AP203e2
      -DFILE_NAME=ap203e2_surface_models.stp
      -DSOLID_NAME=unused_ap203e2_solid
      -DTOLERANCE=0.004
      "-DAPPLICATION=configuration controlled 3D designs of mechanical parts and assemblies"
      -DPRODUCT_ID=ap203e2_surface_models
      "-DPRODUCT_NAME=AP203e2 Surface Models"
      -DASSOCIATION=SHAPE_DEFINITION_REPRESENTATION
      -DTOP=AP203e2_Surface_Models
      -P ${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_surface_models_baseline.cmake
    )
    brlcad_add_test(
      NAME ap203e2_brep_with_voids
      COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/../ap214/tests/ap214_brep_with_voids.stp
      -DINPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_brep_with_voids.stp
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_brep_with_voids.g
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap203e2_brep_with_voids.json
      -DSCHEMA=AP203_CONFIGURATION_CONTROLLED_3D_DESIGN_OF_MECHANICAL_PARTS_AND_ASSEMBLIES_MIM_LF
      -DSCHEMA_LABEL=AP203e2
      -DFILE_NAME=ap203e2_brep_with_voids.stp
      "-DAPPLICATION=configuration controlled 3D designs of mechanical parts and assemblies"
      -DPRODUCT_ID=ap203e2_void_tetra
      "-DPRODUCT_NAME=AP203e2 Void Tetra"
      -DASSOCIATION=SHAPE_DEFINITION_REPRESENTATION
      -DTOP=AP203e2_Void_Tetra
      -P ${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_brep_with_voids_baseline.cmake
    )
  endif()

endif(BRLCAD_ENABLE_STEP)

cmakefiles(
  ${ap203e2g_SOURCES}
  ${gap203e2_SOURCES}
  CMakeLists.txt
  AP203e2.h
  ap203e2.exp
  tests/ap203e2_csg.cmake
  tests/ap203e2_csg.stp
  tests/ap203e2_mapped_assembly.stp
  tests/ap203e2_swept_solids.cmake
  tests/ap203e2_swept_solids.stp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
