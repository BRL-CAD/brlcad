# AP242 editions retain the same long-form FILE_SCHEMA identifier while their
# entity sets and some Part 21 attribute layouts differ.  Build isolated
# generated bindings for each available edition while compiling the same
# converter source against all of them.
set(STEP_SCHEMA_E1_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ap242e1.exp)
set(STEP_SCHEMA_E2_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ap242e2.exp)
set(STEP_SCHEMA_E3_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ap242e3.exp)
set(STEP_SCHEMA_E4_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ap242e4.exp)

if(BRLCAD_ENABLE_STEP)
  generate_schema_inputs(
    ${STEP_SCHEMA_E1_FILE} ap242e1g ap242e1
    AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF_E1
  )
  generate_schema_inputs(
    ${STEP_SCHEMA_E1_FILE} gap242e1 ap242e1
    AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF_E1
  )
  generate_schema_inputs(
    ${STEP_SCHEMA_E2_FILE} ap242e2g ap242e2
    AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF_E2
  )
  generate_schema_inputs(
    ${STEP_SCHEMA_E2_FILE} gap242e2 ap242e2
    AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF_E2
  )
  generate_schema_inputs(
    ${STEP_SCHEMA_E3_FILE} ap242e3g ap242e3
    AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF_E3
  )
  generate_schema_inputs(
    ${STEP_SCHEMA_E3_FILE} gap242e3 ap242e3
    AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF_E3
  )
  generate_schema_inputs(
    ${STEP_SCHEMA_E4_FILE} ap242e4g ap242e4
    AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF_E4
  )
  generate_schema_inputs(
    ${STEP_SCHEMA_E4_FILE} gap242e4 ap242e4
    AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF_E4
  )
endif(BRLCAD_ENABLE_STEP)

set(
  gap242_SOURCES
  ${gap242_express_srcs}
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
  ../g-step/ON_Brep.cpp
  ../g-step/ON_NurbsCurve.cpp
  ../g-step/ON_NurbsSurface.cpp
  ../STEPProductExport.cpp
  ../g-step/Shape_Representation.cpp
  ../g-step/Shape_Representation_Relationship.cpp
  ../g-step/Trees.cpp
  Export.cpp
)

set(
  ap242g_SOURCES
  ${ap242g_express_srcs}
  ../BRLCADWrapper.cpp
  ../STEPString.cpp
  ../STEPBudget.cpp
  ../STEPBrepValidation.cpp
  ../STEPWrapper.cpp
  ../STEPWrapperAttributes.cpp
  ../STEPWrapperIO.cpp
  ../STEPNativeCSGImport.cpp
  ../STEPMaterialImport.cpp
  AP242PMI.cpp
  AP242Tessellated.cpp
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
  Factory.cpp
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
  ap242g_LIBS
  libwdb
  librt
  libbu
  libbrep
  ${ap242g_SCHEMA_BINDINGS_TARGET}
  ${STEPCODE_UTILS_LIBRARY}
  ${STEPCODE_EDITOR_LIBRARY}
  ${STEPCODE_DAI_LIBRARY}
  ${STEPCODE_CORE_LIBRARY}
  OPENNURBS::OPENNURBS
)

if(STEPCODE_LAZY_FOUND)
  list(APPEND ap242g_SOURCES ../STEPLazySession.cpp)
  list(APPEND ap242g_LIBS ${STEPCODE_LAZY_LIBRARY})
endif()

set(
  AP242_INCLUDE_DIRS
  ${CMAKE_CURRENT_BINARY_DIR}
  ${ap242g_SCHEMA_OUT_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../step-g
  ${STEPCODE_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src/libbrep # for PullbackCurve.h
)

if(BRLCAD_ENABLE_STEP)
  step_use_import_common_objects(ap242g_TARGET_SOURCES ap242g_SOURCES)
  step_record_schema_sources(AP242E1 IMPORT ap242g_TARGET_SOURCES)
  step_record_schema_sources(AP242E1 EXPORT gap242_SOURCES)
  step_record_schema_sources(AP242E2 IMPORT ap242g_TARGET_SOURCES)
  step_record_schema_sources(AP242E2 EXPORT gap242_SOURCES)
  step_record_schema_sources(AP242E3 IMPORT ap242g_TARGET_SOURCES)
  step_record_schema_sources(AP242E3 EXPORT gap242_SOURCES)
  step_record_schema_sources(AP242E4 IMPORT ap242g_TARGET_SOURCES)
  step_record_schema_sources(AP242E4 EXPORT gap242_SOURCES)

  if (TARGET mged)
    brlcad_add_test(
      NAME ap242_csg
      COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap242_csg.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap242_csg.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap242_csg.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap242_csg.cmake
    )
  endif()

  if (TARGET mged AND TARGET nirt)
    brlcad_add_test(
      NAME ap242_swept_solids
      COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DNIRT=$<TARGET_FILE:nirt>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap242_swept_solids.stp
      -DWORK_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap242_swept_solids.cmake
    )
  endif()

  if (TARGET mged)
    brlcad_add_test(
      NAME ap242_tessellated
      COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap242_tessellated.stp
      -DOPEN_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap242_tessellated_open_solid.stp
      -DMALFORMED_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap242_tessellated_malformed.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap242_tessellated.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap242_tessellated.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap242_tessellated.cmake
    )
    brlcad_add_test(
      NAME ap242_pmi
      COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap242_pmi.stp
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap242_pmi.json
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap242_pmi.g
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap242_pmi.cmake
    )
    brlcad_add_test(
      NAME ap242_pmi_uncovered
      COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DTEXT_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap242_pmi_text.stp
      -DPOINT_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap242_pmi_points.stp
      -DRETAINED_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap242_pmi_retained_families.stp
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ap242_pmi_uncovered.cmake
    )
    brlcad_add_test(
      NAME ap242_mapped_assembly
      COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/ap242_mapped_assembly.stp
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap242_mapped_assembly.g
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap242_mapped_assembly.json
      -DSTRICT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap242_mapped_assembly_strict.json
      -DTOP=AP242_Pair
      -DCHILD=AP242_Wedge_Block
      -DASSOCIATION=PROPERTY_DEFINITION_REPRESENTATION
      -DSCHEMA_METADATA=STEP::AP242::FILE_SCHEMA
      -DTOLERANCE=0.002
      "-DTRANSFORM_MARKER={1 0 0 0  0 1 0 32  0 0 1 5"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_mapped_assembly_baseline.cmake
    )
    brlcad_add_test(
      NAME ap242_direct_advanced_brep
      COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DTEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_direct_advanced_brep.stp.in
      -DINPUT=${CMAKE_CURRENT_BINARY_DIR}/ap242_direct_advanced_brep.stp
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap242_direct_advanced_brep.g
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap242_direct_advanced_brep.json
      -DSCHEMA=AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF
      -DSCHEMA_LABEL=AP242
      -DFILE_NAME=ap242_direct_advanced_brep.stp
      -DSOLID_NAME=AP242_direct_tetrahedron
      -DTOLERANCE=0.003
      "-DAPPLICATION=managed model based 3D engineering"
      -DPRODUCT_ID=ap242_direct_tetra
      "-DPRODUCT_NAME=AP242 Direct Tetra"
      -DASSOCIATION=PROPERTY_DEFINITION_REPRESENTATION
      -DTOP=AP242_Direct_Tetra
      -P ${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_direct_advanced_brep.cmake
    )
    brlcad_add_test(
      NAME ap242_invalid_brep_policy
      COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DTEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_invalid_surface_brep.stp.in
      -DINPUT=${CMAKE_CURRENT_BINARY_DIR}/ap242_invalid_surface_brep.stp
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap242_invalid_surface_brep.g
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap242_invalid_surface_brep.json
      -DSCHEMA=AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF
      -DSCHEMA_LABEL=AP242
      -DFILE_NAME=ap242_invalid_surface_brep.stp
      "-DAPPLICATION=managed model based 3D engineering"
      -DPRODUCT_ID=ap242_invalid
      "-DPRODUCT_NAME=AP242 Invalid"
      -DASSOCIATION=PROPERTY_DEFINITION_REPRESENTATION
      -P ${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_invalid_brep_policy.cmake
    )
  endif()

  brlcad_add_test(
    NAME ap242_policy_outcomes
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DUNSUPPORTED_TEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_unsupported_product_geometry.stp.in
      -DMALFORMED_TEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_malformed_product_representation.stp.in
      -DEMPTY_TEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_non_geometric_product.stp.in
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -DSCHEMA=AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF
      -DSCHEMA_LABEL=AP242
      "-DAPPLICATION=managed model based 3D engineering"
      -DASSOCIATION=PROPERTY_DEFINITION_REPRESENTATION
      -P ${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_schema_policy_outcomes.cmake
  )
  brlcad_add_test(
    NAME ap242_partial_policy
    COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DTEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_direct_advanced_brep.stp.in
      -DINPUT=${CMAKE_CURRENT_BINARY_DIR}/ap242_partial_policy.stp
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap242_partial_policy.g
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap242_partial_policy.json
      -DSTRICT_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap242_partial_policy_strict.g
      -DSTRICT_REPORT=${CMAKE_CURRENT_BINARY_DIR}/ap242_partial_policy_strict.json
      -DSCHEMA=AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF
      -DSCHEMA_LABEL=AP242
      -DFILE_NAME=ap242_partial_policy.stp
      -DSOLID_NAME=AP242_partial_tetrahedron
      -DTOLERANCE=0.003
      "-DAPPLICATION=managed model based 3D engineering"
      -DPRODUCT_ID=ap242_partial
      "-DPRODUCT_NAME=AP242 Partial"
      -DASSOCIATION=PROPERTY_DEFINITION_REPRESENTATION
      -P ${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_partial_policy_outcome.cmake
  )

  if (TARGET mged)
    brlcad_add_test(
      NAME ap242_wireframe
      COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DTEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_wireframe.stp.in
      -DINPUT=${CMAKE_CURRENT_BINARY_DIR}/ap242_wireframe.stp
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap242_wireframe.g
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap242_wireframe.json
      -DSCHEMA=AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF
      -DSCHEMA_LABEL=AP242
      -DFILE_NAME=ap242_wireframe.stp
      -DTOLERANCE=0.007
      "-DAPPLICATION=managed model based 3D engineering"
      -DPRODUCT_ID=ap242_wire_product
      "-DPRODUCT_NAME=AP242 Wire Product"
      -DASSOCIATION=PROPERTY_DEFINITION_REPRESENTATION
      -DTOP=AP242_Wire_Product
      -P ${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_wireframe_baseline.cmake
    )
    brlcad_add_test(
      NAME ap242_surface_models
      COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DTEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_direct_advanced_brep.stp.in
      -DINPUT=${CMAKE_CURRENT_BINARY_DIR}/ap242_surface_models.stp
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap242_surface_models.g
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap242_surface_models.json
      -DSCHEMA=AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF
      -DSCHEMA_LABEL=AP242
      -DFILE_NAME=ap242_surface_models.stp
      -DSOLID_NAME=unused_ap242_solid
      -DTOLERANCE=0.003
      "-DAPPLICATION=managed model based 3D engineering"
      -DPRODUCT_ID=ap242_surface_models
      "-DPRODUCT_NAME=AP242 Surface Models"
      -DASSOCIATION=PROPERTY_DEFINITION_REPRESENTATION
      -DTOP=AP242_Surface_Models
      -P ${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_surface_models_baseline.cmake
    )
    brlcad_add_test(
      NAME ap242_brep_with_voids
      COMMAND
      ${CMAKE_COMMAND}
      -DSTEP_G=$<TARGET_FILE:step-g>
      -DMGED=$<TARGET_FILE:mged>
      -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/../ap214/tests/ap214_brep_with_voids.stp
      -DINPUT=${CMAKE_CURRENT_BINARY_DIR}/ap242_brep_with_voids.stp
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ap242_brep_with_voids.g
      -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/ap242_brep_with_voids.json
      -DSCHEMA=AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF
      -DSCHEMA_LABEL=AP242
      -DFILE_NAME=ap242_brep_with_voids.stp
      "-DAPPLICATION=managed model based 3D engineering"
      -DPRODUCT_ID=ap242_void_tetra
      "-DPRODUCT_NAME=AP242 Void Tetra"
      -DASSOCIATION=PROPERTY_DEFINITION_REPRESENTATION
      -DTOP=AP242_Void_Tetra
      -P ${CMAKE_CURRENT_SOURCE_DIR}/../tests/step_brep_with_voids_baseline.cmake
    )
  endif()

endif(BRLCAD_ENABLE_STEP)

cmakefiles(
  ${ap242g_SOURCES}
  ${gap242_SOURCES}
  AP242.h
  AP242Tessellated.h
  AP242PMI.h
  CMakeLists.txt
  GlobalUnitAssignedContext.cpp
  GlobalUnitAssignedContext.h
  MeasureValue.cpp
  MeasureValue.h
  MeasureWithUnit.cpp
  MeasureWithUnit.h
  PropertyDefinitionRepresentation.cpp
  PropertyDefinitionRepresentation.h
  RepresentedDefinition.cpp
  RepresentedDefinition.h
  ap242e1.exp
  ap242e2.exp
  ap242e3.exp
  ap242e4.exp
  tests/ap242_csg.cmake
  tests/ap242_csg.stp
  tests/ap242_mapped_assembly.stp
  tests/ap242_pmi.cmake
  tests/ap242_pmi.stp
  tests/ap242_pmi_points.stp
  tests/ap242_pmi_retained_families.stp
  tests/ap242_pmi_text.stp
  tests/ap242_pmi_uncovered.cmake
  tests/ap242_swept_solids.cmake
  tests/ap242_swept_solids.stp
  tests/ap242_tessellated.cmake
  tests/ap242_tessellated.stp
  tests/ap242_tessellated_malformed.stp
  tests/ap242_tessellated_open_solid.stp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
