# Converters may append to existing databases. Each run needs fresh fixtures.
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
file(WRITE "${WORK_DIR}/input.unknown" "solid triangle\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid triangle\n")
function(convert)
  execute_process(COMMAND "${GCV}" ${ARGN} RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE messages)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "Conversion failed (${status}): ${output}\n${messages}")
  endif()
endfunction()
convert("${WORK_DIR}/input.unknown" "${WORK_DIR}/model.g")
configure_file("${WORK_DIR}/model.g" "${WORK_DIR}/database.unknown" COPYONLY)
convert(--output-trait stl.serialization=text "${WORK_DIR}/database.unknown" "${WORK_DIR}/roundtrip.stl")
file(READ "${WORK_DIR}/roundtrip.stl" result)
if(NOT result MATCHES "facet normal")
  message(FATAL_ERROR "STL round trip did not contain geometry")
endif()
convert(--input-format model/stl --output-format model/vnd.brlcad "${WORK_DIR}/input.unknown" "${WORK_DIR}/explicit.payload")
file(WRITE "${WORK_DIR}/plugin.unknown" "MIMETEST")
convert("${WORK_DIR}/plugin.unknown" "${WORK_DIR}/plugin.g")
if(NOT EXISTS "${WORK_DIR}/plugin.g")
  message(FATAL_ERROR "Runtime plugin type was not converted")
endif()

convert(--input-handler "STL Reader" "${WORK_DIR}/input.unknown" "${WORK_DIR}/handler.g")

file(WRITE "${WORK_DIR}/input.OBJ" "o triangle\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n")
convert("${WORK_DIR}/input.OBJ" "${WORK_DIR}/obj.g")
if(HAS_ASSETIMPORT)
  file(READ "${WORK_DIR}/input.unknown" stl_text)
  string(ASCII 239 187 191 utf8_bom)
  file(WRITE "${WORK_DIR}/stl-bom.payload" "${utf8_bom}${stl_text}")
  execute_process(COMMAND "${GCV}" --identify "${WORK_DIR}/stl-bom.payload"
    RESULT_VARIABLE stl_bom_status OUTPUT_VARIABLE stl_bom_stdout ERROR_VARIABLE stl_bom_stderr)
  set(stl_bom_identity "${stl_bom_stdout}${stl_bom_stderr}")
  if(NOT stl_bom_status EQUAL 0 OR NOT stl_bom_identity MATCHES "stl.serialization=text" OR
     NOT stl_bom_identity MATCHES "stl.utf8-bom=true" OR NOT stl_bom_identity MATCHES "encoding=utf-8")
    message(FATAL_ERROR "STL BOM source observations are incorrect: ${stl_bom_identity}")
  endif()
  convert("${WORK_DIR}/stl-bom.payload" "${WORK_DIR}/stl-bom.g")
  file(WRITE "${WORK_DIR}/amf.payload" [=[<?xml version="1.0"?>
<amf version="1.1" unit="millimeter"><object id="0"><mesh><vertices>
<vertex><coordinates><x>0</x><y>0</y><z>0</z></coordinates></vertex>
<vertex><coordinates><x>1</x><y>0</y><z>0</z></coordinates></vertex>
<vertex><coordinates><x>0</x><y>1</y><z>0</z></coordinates></vertex>
</vertices><volume><triangle><v1>0</v1><v2>1</v2><v3>2</v3></triangle></volume>
</mesh></object></amf>
]=])
  execute_process(COMMAND "${GCV}" --identify "${WORK_DIR}/amf.payload"
    RESULT_VARIABLE amf_status OUTPUT_VARIABLE amf_stdout ERROR_VARIABLE amf_stderr)
  set(amf_identity "${amf_stdout}${amf_stderr}")
  if(NOT amf_status EQUAL 0 OR NOT amf_identity MATCHES "model/amf" OR
     NOT amf_identity MATCHES "container=none" OR NOT amf_identity MATCHES "amf.unit-declaration=millimeter")
    message(FATAL_ERROR "AMF source observations are incorrect: ${amf_identity}")
  endif()
  convert("${WORK_DIR}/amf.payload" "${WORK_DIR}/amf.g")
  convert(--output-format application/vnd.brlcad.script "${WORK_DIR}/amf.g" "${WORK_DIR}/amf.asc")
  file(READ "${WORK_DIR}/amf.asc" amf_geometry)
  if(NOT amf_geometry MATCHES " bot " OR NOT amf_geometry MATCHES "F [{][ ]*[{][ ]*0[ ]+1[ ]+2[ ]*[}][ ]*[}]")
    message(FATAL_ERROR "AMF conversion did not retain the triangle")
  endif()
  convert(--input-handler "Assetimport Reader" "${WORK_DIR}/input.unknown" "${WORK_DIR}/asset.g")
  file(REMOVE "${WORK_DIR}/legacy-asset.g")
  convert(--input-format assetimport "${WORK_DIR}/input.unknown" "${WORK_DIR}/legacy-asset.g")
  file(REMOVE "${WORK_DIR}/ap214.step" "${WORK_DIR}/ap203.step")
  convert(--output-format model/step --output-trait step.ap=214 --output-handler "Assetimport Writer"
    "${WORK_DIR}/model.g" "${WORK_DIR}/ap214.step")
  execute_process(COMMAND "${GCV}" --identify "${WORK_DIR}/ap214.step"
    RESULT_VARIABLE step_status ERROR_VARIABLE step_identity)
  if(NOT step_status EQUAL 0 OR NOT step_identity MATCHES "model/step" OR NOT step_identity MATCHES "step.ap=214")
    message(FATAL_ERROR "STEP AP214 export did not identify its declared profile: ${step_identity}")
  endif()
  execute_process(COMMAND "${GCV}" --output-format model/step --output-trait step.ap=203 --output-handler "Assetimport Writer"
    "${WORK_DIR}/model.g" "${WORK_DIR}/ap203.step" RESULT_VARIABLE step_status ERROR_VARIABLE step_messages)
  if(step_status EQUAL 0 OR EXISTS "${WORK_DIR}/ap203.step")
    message(FATAL_ERROR "STEP exporter advertised a profile it does not emit: ${step_messages}")
  endif()
  file(REMOVE "${WORK_DIR}/scene.pbrt" "${WORK_DIR}/relative.pbrt" "${WORK_DIR}/version3.pbrt"
    "${WORK_DIR}/invalid-pbrt.payload" "${WORK_DIR}/invalid-pbrt.pbrt" "${WORK_DIR}/ase.g" "${WORK_DIR}/ase.asc")
  convert(--output-format model/vnd.pbrt.scene --output-trait version=4 --output-handler "Assetimport Writer"
    "${WORK_DIR}/model.g" "${WORK_DIR}/scene.pbrt")
  file(READ "${WORK_DIR}/scene.pbrt" pbrt_scene)
  if(NOT pbrt_scene MATCHES "WorldBegin" OR NOT pbrt_scene MATCHES "trianglemesh")
    message(FATAL_ERROR "PBRT output did not contain scene geometry")
  endif()
  execute_process(COMMAND "${GCV}" --output-format model/vnd.pbrt.scene --output-trait version=4
    --output-handler "Assetimport Writer" "${WORK_DIR}/model.g" relative.pbrt
    WORKING_DIRECTORY "${WORK_DIR}" RESULT_VARIABLE pbrt_status ERROR_VARIABLE pbrt_messages)
  if(NOT pbrt_status EQUAL 0 OR NOT EXISTS "${WORK_DIR}/relative.pbrt")
    message(FATAL_ERROR "PBRT output did not honor the relative destination: ${pbrt_messages}")
  endif()
  execute_process(COMMAND "${GCV}" --output-format model/vnd.pbrt.scene --output-trait version=3
    --output-handler "Assetimport Writer" "${WORK_DIR}/model.g" "${WORK_DIR}/version3.pbrt"
    RESULT_VARIABLE pbrt_status ERROR_VARIABLE pbrt_messages)
  if(pbrt_status EQUAL 0 OR EXISTS "${WORK_DIR}/version3.pbrt")
    message(FATAL_ERROR "PBRT output ignored the requested version: ${pbrt_messages}")
  endif()
  execute_process(COMMAND "${GCV}" --output-format model/vnd.pbrt.scene --output-handler "Assetimport Writer"
    "${WORK_DIR}/model.g" "${WORK_DIR}/invalid-pbrt.payload"
    RESULT_VARIABLE pbrt_status ERROR_VARIABLE pbrt_messages)
  if(pbrt_status EQUAL 0 OR EXISTS "${WORK_DIR}/invalid-pbrt.payload" OR EXISTS "${WORK_DIR}/invalid-pbrt.pbrt")
    message(FATAL_ERROR "PBRT output silently changed the requested filename: ${pbrt_messages}")
  endif()
  file(WRITE "${WORK_DIR}/ase.payload" [=[*3DSMAX_ASCIIEXPORT 200
*MATERIAL_LIST {
  *MATERIAL_COUNT 1
  *MATERIAL 0 {
    *MATERIAL_NAME "default"
    *MATERIAL_DIFFUSE 1.0 1.0 1.0
  }
}
*GEOMOBJECT {
  *NODE_NAME "triangle"
  *NODE_TM {
    *TM_ROW0 1.0 0.0 0.0
    *TM_ROW1 0.0 1.0 0.0
    *TM_ROW2 0.0 0.0 1.0
    *TM_ROW3 0.0 0.0 0.0
  }
  *MESH {
    *TIMEVALUE 0
    *MESH_NUMVERTEX 3
    *MESH_NUMFACES 1
    *MESH_VERTEX_LIST {
      *MESH_VERTEX 0 0.0 0.0 0.0
      *MESH_VERTEX 1 1.0 0.0 0.0
      *MESH_VERTEX 2 0.0 1.0 0.0
    }
    *MESH_FACE_LIST {
      *MESH_FACE 0: A: 0 B: 1 C: 2 AB: 1 BC: 1 CA: 1 *MESH_SMOOTHING 1 *MESH_MTLID 0
    }
  }
  *MATERIAL_REF 0
}
]=])
  execute_process(COMMAND "${GCV}" --identify "${WORK_DIR}/ase.payload"
    RESULT_VARIABLE ase_status ERROR_VARIABLE ase_identity)
  if(NOT ase_status EQUAL 0 OR NOT ase_identity MATCHES "model/vnd.autodesk.ase" OR NOT ase_identity MATCHES "version=200")
    message(FATAL_ERROR "ASE header did not identify its declared format version: ${ase_identity}")
  endif()
  convert(--input-handler "Assetimport Reader" "${WORK_DIR}/ase.payload" "${WORK_DIR}/ase.g")
  convert(--output-format application/vnd.brlcad.script "${WORK_DIR}/ase.g" "${WORK_DIR}/ase.asc")
  file(READ "${WORK_DIR}/ase.asc" ase_geometry)
  if(NOT ase_geometry MATCHES " bot " OR NOT ase_geometry MATCHES "F [{][ ]*[{][ ]*0[ ]+1[ ]+2[ ]*[}][ ]*[}]")
    message(FATAL_ERROR "ASE conversion did not retain the triangle")
  endif()
  convert(--output-format model/obj --output-handler "Assetimport Writer" "${WORK_DIR}/model.g" "${WORK_DIR}/export.payload")
  file(READ "${WORK_DIR}/export.payload" result)
  if(NOT result MATCHES "v[ \t]+[0-9]" OR NOT result MATCHES "f[ \t]+[0-9]")
    message(FATAL_ERROR "Multi-format writer did not honor the requested MIME type")
  endif()
endif()

foreach(kind vtk vrml obj)
  if(kind STREQUAL "vtk")
    set(sample "vtk/mesh.vtk")
  elseif(kind STREQUAL "vrml")
    set(sample "vrml/vrml_example.wrl")
  else()
    set(sample "obj/tetra.obj")
  endif()
  configure_file("${SOURCE_DIR}/regress/gcv/${sample}" "${WORK_DIR}/${kind}.payload" COPYONLY)
  file(REMOVE "${WORK_DIR}/${kind}-renamed.g")
  convert("${WORK_DIR}/${kind}.payload" "${WORK_DIR}/${kind}-renamed.g")
endforeach()
execute_process(COMMAND "${GCV}" --list-formats RESULT_VARIABLE status ERROR_VARIABLE formats)
if(NOT status EQUAL 0 OR NOT formats MATCHES "model/obj.*OBJ Reader")
  message(FATAL_ERROR "Runtime handler listing failed: ${formats}")
endif()
if(HAS_ASSETIMPORT AND formats MATCHES "model/vnd[.]idsoftware[.]bsp[^\n]*Assetimport Reader")
  message(FATAL_ERROR "The ZIP-based SDK driver incorrectly advertises raw BSP input")
endif()
if(HAS_ASSETIMPORT AND formats MATCHES "model/vnd[.]epicgames[.]unreal[.]mesh[^\n]*Assetimport Reader")
  if(NOT formats MATCHES "unreal.vertex-stride=4" OR
     NOT formats MATCHES "text/vnd[.]epicgames[.]unrealscript[^\n]*Assetimport Reader")
    message(FATAL_ERROR "SDK Unreal component/script profiles are missing from runtime discovery")
  endif()
endif()
if(NOT formats MATCHES "model/jt.*JT Writer" OR NOT formats MATCHES "version=8.0")
  message(FATAL_ERROR "JT output profiles are missing from runtime discovery: ${formats}")
endif()
if(HAS_ASSETIMPORT)
  if(NOT formats MATCHES "model/vnd[.]ogre[.]mesh[^\n]*Assetimport Reader[^\n]*ogre.serializer=" OR
     NOT formats MATCHES "model/vnd[.]ogre[.]mesh[+]xml[^\n]*Assetimport Reader[^\n]*encoding=utf-8")
    message(FATAL_ERROR "SDK Ogre profiles are missing from runtime discovery")
  endif()
  if(NOT formats MATCHES "model/vnd[.]realitywave[.]xgl[+]xml[^\n]*Assetimport Reader[^\n]*compression=none" OR
     NOT formats MATCHES "model/vnd[.]realitywave[.]zgl[^\n]*Assetimport Reader[^\n]*xml.encoding=utf-8")
    message(FATAL_ERROR "SDK XGL/ZGL profiles are missing from runtime discovery")
  endif()
  if(NOT formats MATCHES "model/vnd[.]irrlicht[.]mesh[+]xml[^\n]*Assetimport Reader[^\n]*irrlicht.vertex-layouts" OR
     NOT formats MATCHES "model/vnd[.]irrlicht[.]scene[+]xml[^\n]*Assetimport Reader[^\n]*irrlicht.node-types" OR
     NOT formats MATCHES "model/vnd[.]collada[+]xml[^\n]*Assetimport Reader[^\n]*collada.schema-version=1.4")
    message(FATAL_ERROR "SDK XML profiles are missing from runtime discovery")
  endif()
  if(NOT formats MATCHES "model/vnd[.]collada[+]xml[+]zip[^\n]*Assetimport Reader[^\n]*container=zip")
    message(FATAL_ERROR "SDK ZAE profiles are missing from runtime discovery")
  endif()
  set(irr_vertices "0 0 0 0 0 1 ffffffff 0 0\n1 0 0 0 0 1 ffffffff 1 0\n0 1 0 0 0 1 ffffffff 0 1\n")
  set(irr_material "<material><enum name='Type' value='solid'/></material>")
  set(irr_buffer "<buffer>${irr_material}<vertices type='standard' vertexCount='3'>${irr_vertices}</vertices><indices indexCount='3'>0 1 2</indices></buffer>")
  set(irr_root "<?xml version='1.0'?><mesh xmlns='http://irrlicht.sourceforge.net/IRRMESH_09_2007' version='1.0'>")
  file(WRITE "${WORK_DIR}/irr-triangle.xml" "${irr_root}${irr_buffer}</mesh>")
  file(REMOVE "${WORK_DIR}/irr-triangle.g" "${WORK_DIR}/irr-unsupported.g")
  convert(--input-handler "Assetimport Reader" --input-trait "irrlicht.vertex-layouts[]=[\"standard\"]"
    "${WORK_DIR}/irr-triangle.xml" "${WORK_DIR}/irr-triangle.g")
  string(REPEAT " " 70000 irr_padding)
  string(REPLACE "type='standard'" "type='unsupported'" irr_bad_buffer "${irr_buffer}")
  file(WRITE "${WORK_DIR}/irr-unsupported.irrmesh" "${irr_root}${irr_buffer}<!--${irr_padding}-->${irr_bad_buffer}</mesh>")
  execute_process(COMMAND "${GCV}" --input-format model/vnd.irrlicht.mesh+xml --input-handler "Assetimport Reader"
    "${WORK_DIR}/irr-unsupported.irrmesh" "${WORK_DIR}/irr-unsupported.g" RESULT_VARIABLE irr_status ERROR_VARIABLE irr_messages)
  if(irr_status EQUAL 0 OR EXISTS "${WORK_DIR}/irr-unsupported.g")
    message(FATAL_ERROR "Unknown Irrlicht vertex layout was discarded during conversion: ${irr_messages}")
  endif()
  if(NOT formats MATCHES "model/vnd.quick3d[^\n]*Assetimport Reader[^\n]*quick3d.kind=object" OR
     NOT formats MATCHES "model/vnd.quick3d[^\n]*Assetimport Reader[^\n]*quick3d.kind=scene")
    message(FATAL_ERROR "SDK Quick3D profiles are missing from runtime discovery")
  endif()
  if(formats MATCHES "model/vnd.mikumikudance.pmx[^\n]*Assetimport Reader" AND
     NOT formats MATCHES "model/vnd.mikumikudance.pmx[^\n]*Assetimport Reader[^\n]*pmx.additional-uv=0")
    message(FATAL_ERROR "SDK PMX profiles are missing their supported UV layout")
  endif()
  if(formats MATCHES "model/vnd.mikumikudance.pmx[^\n]*Assetimport Reader[^\n]*pmx.additional-uv=[1-9]")
    message(FATAL_ERROR "SDK PMX extra-UV layouts are advertised without working conversion")
  endif()
  if(NOT formats MATCHES "model/vnd.povray.raw[^\n]*Assetimport Reader[^\n]*raw.features" OR
     NOT formats MATCHES "image/x-lws[^\n]*Assetimport Reader[^\n]*encoding=ascii")
    message(FATAL_ERROR "SDK raw triangle or LightWave scene profiles are missing")
  endif()
  foreach(motion_type model/vnd.biovision.bvh model/vnd.autodesk.csm model/vnd.newtek.motion)
    if(formats MATCHES "${motion_type}[^\n]*Assetimport Reader")
      message(FATAL_ERROR "Motion-only SDK data is advertised as geometry conversion: ${motion_type}")
    endif()
  endforeach()
  set(raw_triangle "0 0 0 1 0 0 0 1 0\n")
  file(WRITE "${WORK_DIR}/triangles.raw" "${raw_triangle}")
  file(REMOVE "${WORK_DIR}/raw-triangle.g" "${WORK_DIR}/raw-plate.g")
  convert(--input-handler "Assetimport Reader" --input-trait "raw.features[]=[]"
    "${WORK_DIR}/triangles.raw" "${WORK_DIR}/raw-triangle.g")
  string(REPEAT "\n" 70000 raw_padding)
  file(WRITE "${WORK_DIR}/plate.raw" "${raw_triangle}${raw_padding}part thickf=0.5\n${raw_triangle}")
  execute_process(COMMAND "${GCV}" --input-format model/vnd.povray.raw --input-handler "Assetimport Reader"
    "${WORK_DIR}/plate.raw" "${WORK_DIR}/raw-plate.g" RESULT_VARIABLE raw_status ERROR_VARIABLE raw_messages)
  if(raw_status EQUAL 0 OR EXISTS "${WORK_DIR}/raw-plate.g")
    message(FATAL_ERROR "IABG plate attributes were discarded by the raw triangle reader: ${raw_messages}")
  endif()
  if(NOT formats MATCHES "model/prs.haines.nff[^\n]*Assetimport Reader[^\n]*nff.extensions" OR
     NOT formats MATCHES "model/vnd.sense8.nff[^\n]*Assetimport Reader[^\n]*encoding=ascii")
    message(FATAL_ERROR "SDK NFF dialect profiles are missing from runtime discovery")
  endif()
  set(nff_polygon "p 3\n0 0 0\n1 0 0\n0 1 0\n")
  file(WRITE "${WORK_DIR}/haines.nff" "${nff_polygon}")
  file(WRITE "${WORK_DIR}/sense8.enff" "nff\nversion 2.0\ntriangle\n3\n0 0 0\n1 0 0\n0 1 0\n1\n3 0 1 2\n")
  foreach(nff_family haines sense8)
    if(nff_family STREQUAL "haines")
      set(nff_suffix nff)
    else()
      set(nff_suffix enff)
    endif()
    file(REMOVE "${WORK_DIR}/${nff_family}.g")
    convert(--input-handler "Assetimport Reader" "${WORK_DIR}/${nff_family}.${nff_suffix}" "${WORK_DIR}/${nff_family}.g")
  endforeach()
  file(WRITE "${WORK_DIR}/unversioned.enff" "nff\ntriangle\n3\n0 0 0\n1 0 0\n0 1 0\n1\n3 0 1 2\n")
  file(REMOVE "${WORK_DIR}/unversioned.g" "${WORK_DIR}/asserted-version.g")
  convert(--input-handler "Assetimport Reader" "${WORK_DIR}/unversioned.enff" "${WORK_DIR}/unversioned.g")
  execute_process(COMMAND "${GCV}" --input-format model/vnd.sense8.nff --input-handler "Assetimport Reader"
    --input-trait version=2.0 "${WORK_DIR}/unversioned.enff" "${WORK_DIR}/asserted-version.g"
    RESULT_VARIABLE nff_status ERROR_VARIABLE nff_messages)
  if(nff_status EQUAL 0 OR EXISTS "${WORK_DIR}/asserted-version.g")
    message(FATAL_ERROR "Sense8 input without a version satisfied an explicit version assertion: ${nff_messages}")
  endif()
  string(REPEAT "# NFF profile padding\n" 4000 nff_padding)
  file(WRITE "${WORK_DIR}/unsupported.nff" "${nff_polygon}${nff_padding}pb 4\n")
  file(REMOVE "${WORK_DIR}/unsupported-nff.g")
  execute_process(COMMAND "${GCV}" --input-format model/prs.haines.nff --input-handler "Assetimport Reader"
    "${WORK_DIR}/unsupported.nff" "${WORK_DIR}/unsupported-nff.g" RESULT_VARIABLE nff_status ERROR_VARIABLE nff_messages)
  if(nff_status EQUAL 0 OR EXISTS "${WORK_DIR}/unsupported-nff.g")
    message(FATAL_ERROR "Unsupported NFF extension was not rejected before output: ${nff_messages}")
  endif()
  if(NOT formats MATCHES "model/vnd.foundry.lxo[^\n]*Assetimport Reader[^\n]*iff.form=LXOB" OR
     NOT formats MATCHES "image/x-lwo[^\n]*Assetimport Reader[^\n]*iff.form=LWO2")
    message(FATAL_ERROR "SDK LightWave/Modo profiles are missing from runtime discovery")
  endif()
  foreach(profile_type model/vnd.idsoftware.mdl model/vnd.conitec.mdl model/vnd.valve.mdl image/vnd.conitec.hmp model/vnd.valve.smd model/vnd.idsoftware.md5)
    if(NOT formats MATCHES "${profile_type}[^\n]*Assetimport Reader[^\n]*version=")
      message(FATAL_ERROR "SDK input profiles are missing from runtime discovery: ${profile_type}")
    endif()
  endforeach()
  file(WRITE "${WORK_DIR}/triangle.smd" "version 1\nnodes\n0 \"origin\" -1\nend\nskeleton\ntime 0\n0 0 0 0 0 0 0\nend\ntriangles\nmaterial\n0 0 0 0 0 0 1 0 0\n0 1 0 0 0 0 1 1 0\n0 0 1 0 0 0 1 0 1\nend\n")
  file(REMOVE "${WORK_DIR}/smd.g" "${WORK_DIR}/unsupported-smd.g")
  convert(--input-handler "Assetimport Reader" "${WORK_DIR}/triangle.smd" "${WORK_DIR}/smd.g")
  string(REPEAT "// profile padding\n" 4000 skeletal_padding)
  file(WRITE "${WORK_DIR}/animation.smd" "version 1\nnodes\n0 \"origin\" -1\nend\n${skeletal_padding}skeleton\ntime 0\n0 0 0 0 0 0 0\nend\n")
  execute_process(COMMAND "${GCV}" --input-format model/vnd.valve.smd --input-handler "Assetimport Reader"
    "${WORK_DIR}/animation.smd" "${WORK_DIR}/unsupported-smd.g" RESULT_VARIABLE skeletal_status ERROR_VARIABLE skeletal_messages)
  if(skeletal_status EQUAL 0 OR EXISTS "${WORK_DIR}/unsupported-smd.g")
    message(FATAL_ERROR "Skeleton-only SMD input was not rejected before output: ${skeletal_messages}")
  endif()
endif()
if(NOT formats MATCHES "model/vnd.mcneel.3dm[^\n]*Rhino Reader[^\n]*version=2")
  message(FATAL_ERROR "3DM archive profiles are missing from runtime discovery: ${formats}")
endif()
if(NOT formats MATCHES "model/vnd.fastgen[^\n]*FASTGEN4 Reader[^\n]*fastgen.dialect=4" OR
   NOT formats MATCHES "model/vnd.fastgen[^\n]*FASTGEN4 Writer[^\n]*fastgen.dialect=4")
  message(FATAL_ERROR "FASTGEN4 dialect profiles are missing from runtime discovery: ${formats}")
endif()
string(REPEAT "        " 4 fastgen_unused_fields)
file(WRITE "${WORK_DIR}/fastgen.payload" "SECTION        1       1       2\nGRID           1             0.0     0.0     0.0\nCSPHERE        1       1       1${fastgen_unused_fields}     1.0\nENDDATA\n")
file(REMOVE "${WORK_DIR}/fastgen.g" "${WORK_DIR}/fastgen-export.payload" "${WORK_DIR}/unsupported-fastgen.g")
convert("${WORK_DIR}/fastgen.payload" "${WORK_DIR}/fastgen.g")
convert(--output-format model/vnd.fastgen --output-handler "FASTGEN4 Writer" --output-trait fastgen.dialect=4
  "${WORK_DIR}/fastgen.g" "${WORK_DIR}/fastgen-export.payload")
execute_process(COMMAND "${GCV}" --identify "${WORK_DIR}/fastgen-export.payload"
  RESULT_VARIABLE fastgen_status ERROR_VARIABLE fastgen_identity)
if(NOT fastgen_status EQUAL 0 OR NOT fastgen_identity MATCHES "model/vnd.fastgen" OR
   NOT fastgen_identity MATCHES "fastgen.dialect=4")
  message(FATAL_ERROR "FASTGEN export did not identify its dialect: ${fastgen_identity}")
endif()
execute_process(COMMAND "${GCV}" --input-trait fastgen.dialect=3 "${WORK_DIR}/fastgen.payload" "${WORK_DIR}/unsupported-fastgen.g"
  RESULT_VARIABLE fastgen_status ERROR_VARIABLE fastgen_messages)
if(fastgen_status EQUAL 0 OR EXISTS "${WORK_DIR}/unsupported-fastgen.g")
  message(FATAL_ERROR "Unsupported FASTGEN dialect was not rejected before output: ${fastgen_messages}")
endif()
configure_file("${SOURCE_DIR}/regress/gcv/rhino/idef_test.3dm" "${WORK_DIR}/rhino.payload" COPYONLY)
file(REMOVE "${WORK_DIR}/rhino.g" "${WORK_DIR}/unsupported-rhino.g")
convert(--input-handler "Rhino Reader" "${WORK_DIR}/rhino.payload" "${WORK_DIR}/rhino.g")
execute_process(COMMAND "${GCV}" --input-handler "Rhino Reader" --input-trait version=999
  "${WORK_DIR}/rhino.payload" "${WORK_DIR}/unsupported-rhino.g"
  RESULT_VARIABLE rhino_status ERROR_VARIABLE rhino_messages)
if(rhino_status EQUAL 0 OR EXISTS "${WORK_DIR}/unsupported-rhino.g")
  message(FATAL_ERROR "Unsupported 3DM archive request was not rejected before output: ${rhino_messages}")
endif()
file(REMOVE "${WORK_DIR}/unsupported-jt.payload")
execute_process(COMMAND "${GCV}" --output-format model/jt --output-handler "JT Writer"
  --output-trait version=11.0 "${WORK_DIR}/model.g" "${WORK_DIR}/unsupported-jt.payload"
  RESULT_VARIABLE jt_status ERROR_VARIABLE jt_messages)
if(jt_status EQUAL 0 OR EXISTS "${WORK_DIR}/unsupported-jt.payload")
  message(FATAL_ERROR "Unsupported JT output version was not rejected before output: ${jt_messages}")
endif()
execute_process(COMMAND "${GCV}" --identify "${WORK_DIR}/obj.payload" RESULT_VARIABLE status ERROR_VARIABLE identified)
if(NOT status EQUAL 0 OR NOT identified MATCHES "model/obj: heuristic")
  message(FATAL_ERROR "OBJ inspection failed: ${identified}")
endif()
file(WRITE "${WORK_DIR}/invalid.payload" "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 0 2 3\n")
file(REMOVE "${WORK_DIR}/invalid.g")
execute_process(COMMAND "${GCV}" "${WORK_DIR}/invalid.payload" "${WORK_DIR}/invalid.g" RESULT_VARIABLE status ERROR_VARIABLE messages)
if(status EQUAL 0 OR EXISTS "${WORK_DIR}/invalid.g")
  message(FATAL_ERROR "Invalid OBJ was accepted: ${messages}")
endif()

file(REMOVE "${WORK_DIR}/binary.stl" "${WORK_DIR}/binary.g" "${WORK_DIR}/unsupported.stl" "${WORK_DIR}/contradiction.g")
convert(--output-trait encoding=binary --output-trait stl.serialization=binary "${WORK_DIR}/model.g" "${WORK_DIR}/binary.stl")
execute_process(COMMAND "${GCV}" --identify "${WORK_DIR}/binary.stl" RESULT_VARIABLE status ERROR_VARIABLE identified)
if(NOT status EQUAL 0 OR NOT identified MATCHES "encoding=binary" OR NOT identified MATCHES "byte-order=little-endian")
  message(FATAL_ERROR "Binary STL request was not honored: ${identified}")
endif()
convert("${WORK_DIR}/binary.stl" "${WORK_DIR}/binary.g")
execute_process(COMMAND "${GCV}" --output-trait encoding=unsupported "${WORK_DIR}/model.g" "${WORK_DIR}/unsupported.stl"
  RESULT_VARIABLE status ERROR_VARIABLE messages)
if(status EQUAL 0 OR EXISTS "${WORK_DIR}/unsupported.stl")
  message(FATAL_ERROR "Unsupported output variant was accepted: ${messages}")
endif()
execute_process(COMMAND "${GCV}" --input-trait encoding=ascii "${WORK_DIR}/binary.stl" "${WORK_DIR}/contradiction.g"
  RESULT_VARIABLE status ERROR_VARIABLE messages)
if(status EQUAL 0 OR EXISTS "${WORK_DIR}/contradiction.g")
  message(FATAL_ERROR "Observed binary STL was overridden by an input assertion: ${messages}")
endif()

function(reject_conversion target)
  file(REMOVE "${target}")
  execute_process(COMMAND "${GCV}" ${ARGN} RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE messages)
  if(NOT status EQUAL 1 OR EXISTS "${target}")
    message(FATAL_ERROR "Expected a clean conversion failure without output, got ${status}: ${output}\n${messages}")
  endif()
endfunction()

file(REMOVE "${WORK_DIR}/native-profile-source.g")
convert("${SOURCE_DIR}/regress/gcv/stl/tetra.stl" "${WORK_DIR}/native-profile-source.g")
foreach(option default vertex-normals usemtl)
  set(extra)
  if(NOT option STREQUAL "default")
    set(extra -O "--${option}")
  endif()
  set(obj "${WORK_DIR}/native-profile-${option}.obj")
  set(database "${WORK_DIR}/native-profile-${option}.g")
  file(REMOVE "${obj}" "${database}" "${WORK_DIR}/native-profile-${option}.mtl")
  convert(--output-handler "OBJ Writer" --output-format model/obj --output-trait encoding=ascii
    -i "${WORK_DIR}/native-profile-source.g" -o "${obj}" ${extra})
  file(READ "${obj}" text)
  if(NOT text MATCHES "\nf ")
    message(FATAL_ERROR "Native OBJ text profile did not contain faces")
  endif()
  convert(--input-handler "OBJ Reader" --input-trait encoding=ascii "${obj}" "${database}")
endforeach()
string(ASCII 195 169 obj_e_acute)
set(obj_unicode "caf${obj_e_acute}")
file(WRITE "${WORK_DIR}/obj-charset.asc"
  "units mm\nput box.s arb8 V1 {0 0 0} V2 {1 0 0} V3 {1 1 0} V4 {0 1 0} V5 {0 0 1} V6 {1 0 1} V7 {1 1 1} V8 {0 1 1}\nput {${obj_unicode}.r} comb region yes tree {l box.s}\n")
convert("${WORK_DIR}/obj-charset.asc" "${WORK_DIR}/obj-charset.g")
convert(--output-handler "OBJ Writer" --output-trait obj.serialization=text
  "${WORK_DIR}/obj-charset.g" "${WORK_DIR}/${obj_unicode}.obj")
file(READ "${WORK_DIR}/${obj_unicode}.obj" obj_text)
if(NOT obj_text MATCHES "\nf " OR NOT obj_text MATCHES "g ${obj_unicode}[.]r" OR
   NOT obj_text MATCHES "mtllib ${obj_unicode}[.]mtl")
  message(FATAL_ERROR "Native OBJ text output did not retain Unicode names and its companion reference")
endif()

reject_conversion("${WORK_DIR}/native-binary.obj" --output-handler "OBJ Writer" --output-format model/obj
  --output-trait encoding=binary "${WORK_DIR}/native-profile-source.g" "${WORK_DIR}/native-binary.obj")
if(EXISTS "${WORK_DIR}/native-binary.mtl")
  message(FATAL_ERROR "Rejected OBJ output profile created a material side file")
endif()
file(REMOVE "${WORK_DIR}/native-profile.igs" "${WORK_DIR}/native-iges-profile.g")
convert(--output-handler "IGES Writer" --output-format application/iges --output-trait encoding=ascii --output-trait version=5.3
  -i "${WORK_DIR}/native-profile-source.g" -o "${WORK_DIR}/native-profile.igs" -O --faceted)
convert(--input-handler "IGES Reader" --input-format model/iges --input-trait version=5.3 --input-trait iges.version-code=11
  "${WORK_DIR}/native-profile.igs" "${WORK_DIR}/native-iges-profile.g")
reject_conversion("${WORK_DIR}/native-iges-old.igs" --output-handler "IGES Writer" --output-format model/iges
  --output-trait version=5.2 "${WORK_DIR}/native-profile-source.g" "${WORK_DIR}/native-iges-old.igs")


if(HAS_ASSETIMPORT)
  set(ifc_source "${SOURCE_DIR}/src/libgcv/tests/ifc_tetra.ifc")
  foreach(suffix ifc step stp unknown)
    configure_file("${ifc_source}" "${WORK_DIR}/tetra.${suffix}" COPYONLY)
    file(REMOVE "${WORK_DIR}/ifc-${suffix}.g")
    convert(--input-handler "Assetimport Reader" "${WORK_DIR}/tetra.${suffix}" "${WORK_DIR}/ifc-${suffix}.g")
  endforeach()
  file(REMOVE "${WORK_DIR}/ifc.asc")
  convert(--output-format application/vnd.brlcad.script "${WORK_DIR}/ifc-ifc.g" "${WORK_DIR}/ifc.asc")
  file(READ "${WORK_DIR}/ifc.asc" ifc_geometry)
  if(NOT ifc_geometry MATCHES " bot ")
    message(FATAL_ERROR "IFC MIME selection did not retain geometry")
  endif()
  execute_process(COMMAND "${GCV}" --identify "${WORK_DIR}/tetra.ifc"
    RESULT_VARIABLE ifc_status ERROR_VARIABLE ifc_identity)
  if(NOT ifc_status EQUAL 0 OR NOT ifc_identity MATCHES "application/p21" OR NOT ifc_identity MATCHES "p21.schema=IFC2X3")
    message(FATAL_ERROR "IFC-SPF did not identify its Part 21 schema: ${ifc_identity}")
  endif()
  reject_conversion("${WORK_DIR}/ifc-wrong-schema.g" --input-format application/p21 --input-trait p21.schema=IFC4
    --input-handler "Assetimport Reader" "${WORK_DIR}/tetra.ifc" "${WORK_DIR}/ifc-wrong-schema.g")
  reject_conversion("${WORK_DIR}/ifc-not-step.g" --input-format model/step
    --input-handler "Assetimport Reader" "${WORK_DIR}/tetra.ifc" "${WORK_DIR}/ifc-not-step.g")
  file(READ "${ifc_source}" ifc_text)
  string(REPLACE "FILE_SCHEMA(('IFC2X3'));" "FILE_SCHEMA(('IFC4'));" ifc_unsupported "${ifc_text}")
  file(WRITE "${WORK_DIR}/ifc4.ifc" "${ifc_unsupported}")
  reject_conversion("${WORK_DIR}/ifc4.g" --input-handler "Assetimport Reader" "${WORK_DIR}/ifc4.ifc" "${WORK_DIR}/ifc4.g")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cf "${WORK_DIR}/tetra.ifczip" --format=zip tetra.ifc
    WORKING_DIRECTORY "${WORK_DIR}" RESULT_VARIABLE ifc_zip_status)
  if(NOT ifc_zip_status EQUAL 0)
    message(FATAL_ERROR "Could not create IFCZIP identity fixture")
  endif()
  reject_conversion("${WORK_DIR}/ifczip.g" --input-format application/zip --input-handler "Assetimport Reader"
    "${WORK_DIR}/tetra.ifczip" "${WORK_DIR}/ifczip.g")
  reject_conversion("${WORK_DIR}/ifczip-legacy.g" --input-format assetimport
    "${WORK_DIR}/tetra.ifczip" "${WORK_DIR}/ifczip-legacy.g")
  configure_file("${ifc_source}" "${WORK_DIR}/plain.ifczip" COPYONLY)
  reject_conversion("${WORK_DIR}/ifczip-plain.g" --input-format application/p21 --input-handler "Assetimport Reader"
    "${WORK_DIR}/plain.ifczip" "${WORK_DIR}/ifczip-plain.g")
  set(zae_files "${WORK_DIR}/zae-files")
  file(MAKE_DIRECTORY "${zae_files}")
  configure_file("${SOURCE_DIR}/regress/gcv/dae/tetra.dae" "${zae_files}/model.dae" COPYONLY)
  file(WRITE "${zae_files}/manifest.xml" "<manifest><dae_root>model.dae</dae_root></manifest>")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cf "${WORK_DIR}/model.zae" --format=zip manifest.xml model.dae
    WORKING_DIRECTORY "${zae_files}" RESULT_VARIABLE zae_status)
  if(NOT zae_status EQUAL 0)
    message(FATAL_ERROR "Cannot construct the ZAE fixture")
  endif()
  file(REMOVE "${WORK_DIR}/zae.g" "${WORK_DIR}/zae.asc" "${WORK_DIR}/zae-plain.g")
  convert(--input-format model/vnd.collada+xml+zip --input-handler "Assetimport Reader"
    --input-trait container=zip "${WORK_DIR}/model.zae" "${WORK_DIR}/zae.g")
  convert(--output-format application/vnd.brlcad.script "${WORK_DIR}/zae.g" "${WORK_DIR}/zae.asc")
  file(READ "${WORK_DIR}/zae.asc" zae_geometry)
  if(NOT zae_geometry MATCHES " bot ")
    message(FATAL_ERROR "ZAE conversion did not retain mesh geometry")
  endif()
  reject_conversion("${WORK_DIR}/zae-xml.g" --input-format model/vnd.collada+xml --input-handler "Assetimport Reader"
    "${WORK_DIR}/model.zae" "${WORK_DIR}/zae-xml.g")
  reject_conversion("${WORK_DIR}/zae-package-version.g" --input-format model/vnd.collada+xml+zip --input-handler "Assetimport Reader"
    --input-trait version=1.4.1 "${WORK_DIR}/model.zae" "${WORK_DIR}/zae-package-version.g")
  configure_file("${zae_files}/model.dae" "${WORK_DIR}/plain.zae" COPYONLY)
  reject_conversion("${WORK_DIR}/zae-not-package.g" --input-format model/vnd.collada+xml+zip --input-handler "Assetimport Reader"
    "${WORK_DIR}/plain.zae" "${WORK_DIR}/zae-not-package.g")
  convert(--input-format model/vnd.collada+xml --input-handler "Assetimport Reader"
    "${WORK_DIR}/plain.zae" "${WORK_DIR}/zae-plain.g")
endif()

file(WRITE "${WORK_DIR}/broken.gltf" "{\"asset\":{\"version\":\"2.0\"},\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":999}}]}]}")
reject_conversion("${WORK_DIR}/broken-gltf.g" --input-handler "GLTF Reader" "${WORK_DIR}/broken.gltf" "${WORK_DIR}/broken-gltf.g")
if(HAS_ASSETIMPORT)
  if(formats MATCHES "application/vnd[.]idsoftware[.]pk3[+]zip[^\n]*Assetimport Reader")
    file(WRITE "${WORK_DIR}/pk3-readme.txt" "This asset package has no map.\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cf "${WORK_DIR}/no-map.pk3" --format=zip pk3-readme.txt
      WORKING_DIRECTORY "${WORK_DIR}" RESULT_VARIABLE pk3_status ERROR_VARIABLE pk3_error)
    if(NOT pk3_status EQUAL 0)
      message(FATAL_ERROR "Cannot create PK3 fixture: ${pk3_error}")
    endif()
    reject_conversion("${WORK_DIR}/no-map.g" --input-handler "Assetimport Reader"
      --input-format application/vnd.idsoftware.pk3+zip --input-trait container=zip
      "${WORK_DIR}/no-map.pk3" "${WORK_DIR}/no-map.g")
    reject_conversion("${WORK_DIR}/wrong-package.g" --input-handler "Assetimport Reader"
      --input-format application/vnd.idsoftware.pk3+zip --input-trait container=rar
      "${WORK_DIR}/no-map.pk3" "${WORK_DIR}/wrong-package.g")
    reject_conversion("${WORK_DIR}/wrong-raw-map.g" --input-handler "Assetimport Reader"
      --input-format model/vnd.idsoftware.bsp "${WORK_DIR}/no-map.pk3" "${WORK_DIR}/wrong-raw-map.g")
  endif()
  set(ogre_geometry "<sharedgeometry vertexcount='3'><vertexbuffer positions='true'><vertex><position x='0' y='0' z='0'/></vertex><vertex><position x='1' y='0' z='0'/></vertex><vertex><position x='0' y='1' z='0'/></vertex></vertexbuffer></sharedgeometry>")
  file(WRITE "${WORK_DIR}/ogre.mesh.xml" "<?xml version='1.0'?><mesh>${ogre_geometry}<submeshes><submesh usesharedvertices='true'><faces count='1'><face v1='0' v2='1' v3='2'/></faces></submesh></submeshes></mesh>")
  file(REMOVE "${WORK_DIR}/ogre.g")
  convert(--input-handler "Assetimport Reader" --input-format model/vnd.ogre.mesh+xml
    --input-trait encoding=utf-8 "${WORK_DIR}/ogre.mesh.xml" "${WORK_DIR}/ogre.g")
  reject_conversion("${WORK_DIR}/ogre-wrong-type.g" --input-handler "Assetimport Reader"
    --input-format model/vnd.ogre.mesh "${WORK_DIR}/ogre.mesh.xml" "${WORK_DIR}/ogre-wrong-type.g")
  reject_conversion("${WORK_DIR}/ogre-wrong-version.g" --input-handler "Assetimport Reader"
    --input-format model/vnd.ogre.mesh+xml --input-trait version=1.0
    "${WORK_DIR}/ogre.mesh.xml" "${WORK_DIR}/ogre-wrong-version.g")
  set(xgl_scene "<?xml version='1.0'?><WORLD><BACKGROUND><BACKCOLOR>1,1,1</BACKCOLOR></BACKGROUND><LIGHTING><AMBIENT>1,1,1</AMBIENT></LIGHTING><MESH><MAT ID='0'><DIFF>1,0,0</DIFF></MAT><P ID='0'>0,0,0</P><P ID='1'>1,0,0</P><P ID='2'>0,1,0</P><F><MATREF>0</MATREF><FV1><PREF>0</PREF></FV1><FV2><PREF>1</PREF></FV2><FV3><PREF>2</PREF></FV3></F></MESH></WORLD>")
  file(WRITE "${WORK_DIR}/xgl.payload" "${xgl_scene}")
  file(REMOVE "${WORK_DIR}/xgl.g")
  convert(--input-handler "Assetimport Reader" --input-format model/vnd.realitywave.xgl+xml
    --input-trait encoding=utf-8 --input-trait compression=none "${WORK_DIR}/xgl.payload" "${WORK_DIR}/xgl.g")
  reject_conversion("${WORK_DIR}/xgl-wrong-compression.g" --input-handler "Assetimport Reader"
    --input-format model/vnd.realitywave.xgl+xml --input-trait compression=zlib
    "${WORK_DIR}/xgl.payload" "${WORK_DIR}/xgl-wrong-compression.g")
  reject_conversion("${WORK_DIR}/xgl-wrong-version.g" --input-handler "Assetimport Reader"
    --input-format model/vnd.realitywave.xgl+xml --input-trait version=1.0
    "${WORK_DIR}/xgl.payload" "${WORK_DIR}/xgl-wrong-version.g")
  file(WRITE "${WORK_DIR}/plain.zgl" "${xgl_scene}")
  reject_conversion("${WORK_DIR}/plain-zgl.g" --input-handler "Assetimport Reader"
    --input-format model/vnd.realitywave.xgl+xml "${WORK_DIR}/plain.zgl" "${WORK_DIR}/plain-zgl.g")
  reject_conversion("${WORK_DIR}/sdk-wrong-input-type.g" --input-handler "Assetimport Reader"
    --input-format model/vnd.autodesk.ase "${WORK_DIR}/input.unknown" "${WORK_DIR}/sdk-wrong-input-type.g")
  reject_conversion("${WORK_DIR}/sdk-wrong-input-option.g" --input-handler "Assetimport Reader"
    -i "${WORK_DIR}/input.unknown" -o "${WORK_DIR}/sdk-wrong-input-option.g" -I --format obj)
  file(REMOVE "${WORK_DIR}/sdk-input-option.g")
  convert(--input-handler "Assetimport Reader" -i "${WORK_DIR}/input.unknown"
    -o "${WORK_DIR}/sdk-input-option.g" -I --format .STL)
  file(REMOVE "${WORK_DIR}/sdk.glb" "${WORK_DIR}/sdk-glb.g" "${WORK_DIR}/sdk-x3d.g" "${WORK_DIR}/sdk-nurbs.g")
  convert(--output-handler "Assetimport Writer" --output-format model/gltf-binary
    --output-trait version=2.0 "${WORK_DIR}/model.g" "${WORK_DIR}/sdk.glb")
  convert(--input-handler "Assetimport Reader" "${WORK_DIR}/sdk.glb" "${WORK_DIR}/sdk-glb.g")
  reject_conversion("${WORK_DIR}/sdk-wrong-gltf-type.g" --input-handler "Assetimport Reader"
    --input-format model/gltf+json "${WORK_DIR}/sdk.glb" "${WORK_DIR}/sdk-wrong-gltf-type.g")
  convert(--input-handler "Assetimport Reader" --input-format model/x3d+xml
    "${SOURCE_DIR}/regress/gcv/x3d/tetra.x3d" "${WORK_DIR}/sdk-x3d.g")
  convert(--input-handler "Assetimport Reader" --input-format model/x3d+xml
    "${SOURCE_DIR}/regress/gcv/x3d/nurbs_patch.x3d" "${WORK_DIR}/sdk-nurbs.g")
  convert(--input-handler "Assetimport Reader" --input-format model/x3d+xml
    --input-trait version=3.3 --input-trait x3d.profile=Interchange --input-trait encoding=utf-8
    "${SOURCE_DIR}/regress/gcv/x3d/tetra.x3d" "${WORK_DIR}/sdk-profile-x3d.g")
  reject_conversion("${WORK_DIR}/sdk-wrong-x3d-version.g" --input-handler "Assetimport Reader"
    --input-format model/x3d+xml --input-trait version=4.0
    "${SOURCE_DIR}/regress/gcv/x3d/tetra.x3d" "${WORK_DIR}/sdk-wrong-x3d-version.g")
  reject_conversion("${WORK_DIR}/sdk-wrong-x3d-profile.g" --input-handler "Assetimport Reader"
    --input-format model/x3d+xml --input-trait x3d.profile=Full
    "${SOURCE_DIR}/regress/gcv/x3d/tetra.x3d" "${WORK_DIR}/sdk-wrong-x3d-profile.g")
  reject_conversion("${WORK_DIR}/sdk-wrong-x3d-encoding.g" --input-handler "Assetimport Reader"
    --input-format model/x3d+xml --input-trait encoding=binary
    "${SOURCE_DIR}/regress/gcv/x3d/tetra.x3d" "${WORK_DIR}/sdk-wrong-x3d-encoding.g")
  reject_conversion("${WORK_DIR}/sdk-wrong-x3d-type.g" --input-handler "Assetimport Reader"
    --input-format model/x3d+fastinfoset "${SOURCE_DIR}/regress/gcv/x3d/tetra.x3d" "${WORK_DIR}/sdk-wrong-x3d-type.g")
  file(REMOVE "${WORK_DIR}/sdk-export.x3d" "${WORK_DIR}/sdk-export-x3d.g")
  convert(--output-handler "Assetimport Writer" --output-format model/x3d+xml
    --output-trait version=3.3 --output-trait encoding=utf-8 --output-trait x3d.profile=Interchange
    "${WORK_DIR}/model.g" "${WORK_DIR}/sdk-export.x3d")
  execute_process(COMMAND "${GCV}" --identify "${WORK_DIR}/sdk-export.x3d"
    RESULT_VARIABLE x3d_status ERROR_VARIABLE x3d_identity)
  if(NOT x3d_status EQUAL 0 OR NOT x3d_identity MATCHES "model/x3d[+]xml" OR
      NOT x3d_identity MATCHES "version=3.3" OR NOT x3d_identity MATCHES "encoding=utf-8" OR
      NOT x3d_identity MATCHES "x3d.profile=Interchange")
    message(FATAL_ERROR "X3D output did not identify its declared profile: ${x3d_identity}")
  endif()
  convert(--input-handler "Assetimport Reader" "${WORK_DIR}/sdk-export.x3d" "${WORK_DIR}/sdk-export-x3d.g")
  foreach(conflict version=4.0 encoding=utf-16le x3d.profile=Full)
    reject_conversion("${WORK_DIR}/sdk-conflicting.x3d" --output-handler "Assetimport Writer"
      --output-format model/x3d+xml --output-trait "${conflict}"
      "${WORK_DIR}/model.g" "${WORK_DIR}/sdk-conflicting.x3d")
  endforeach()
  reject_conversion("${WORK_DIR}/conflicting-export.obj" --output-handler "Assetimport Writer" --output-format model/obj
    -i "${WORK_DIR}/model.g" -o "${WORK_DIR}/conflicting-export.obj" -O --format stl)
  file(REMOVE "${WORK_DIR}/consistent-export.obj")
  convert(--output-handler "Assetimport Writer" --output-format model/obj
    -i "${WORK_DIR}/model.g" -o "${WORK_DIR}/consistent-export.obj" -O --format obj)
endif()
file(REMOVE "${WORK_DIR}/roundtrip.asc" "${WORK_DIR}/asc-roundtrip.g")
convert("${WORK_DIR}/model.g" "${WORK_DIR}/roundtrip.asc")
convert(--input-format application/vnd.brlcad.script "${WORK_DIR}/roundtrip.asc" "${WORK_DIR}/asc-roundtrip.g")
file(REMOVE "${WORK_DIR}/native-v4.g" "${WORK_DIR}/native-v5.g" "${WORK_DIR}/legacy.asc" "${WORK_DIR}/legacy-asc.g")
convert(--output-trait version=4 "${WORK_DIR}/model.g" "${WORK_DIR}/native-v4.g")
execute_process(COMMAND "${GCV}" --identify "${WORK_DIR}/native-v4.g" RESULT_VARIABLE status ERROR_VARIABLE identified)
if(NOT status EQUAL 0 OR NOT identified MATCHES "version=4")
  message(FATAL_ERROR "V4 database request was not honored: ${identified}")
endif()
convert(--output-trait version=5 "${WORK_DIR}/native-v4.g" "${WORK_DIR}/native-v5.g")
file(SHA256 "${WORK_DIR}/native-v4.g" before)
execute_process(COMMAND "${GCV}" --output-trait version=5 "${WORK_DIR}/model.g" "${WORK_DIR}/native-v4.g"
  RESULT_VARIABLE status ERROR_VARIABLE messages)
file(SHA256 "${WORK_DIR}/native-v4.g" after)
if(status EQUAL 0 OR NOT before STREQUAL after)
  message(FATAL_ERROR "Existing database version mismatch was not rejected without changes: ${messages}")
endif()
convert(--output-trait asc.syntax=legacy-records "${WORK_DIR}/model.g" "${WORK_DIR}/legacy.asc")
execute_process(COMMAND "${GCV}" --identify "${WORK_DIR}/legacy.asc" RESULT_VARIABLE status ERROR_VARIABLE identified)
if(NOT status EQUAL 0 OR NOT identified MATCHES "asc.syntax=legacy-records")
  message(FATAL_ERROR "Legacy ASC syntax was not honored: ${identified}")
endif()
configure_file("${WORK_DIR}/legacy.asc" "${WORK_DIR}/legacy.payload" COPYONLY)
convert("${WORK_DIR}/legacy.payload" "${WORK_DIR}/legacy-asc.g")
reject_conversion("${WORK_DIR}/contradicted-asc.g" --input-trait asc.syntax=tcl-commands
  "${WORK_DIR}/legacy.payload" "${WORK_DIR}/contradicted-asc.g")

file(WRITE "${WORK_DIR}/commands.asc" [=[title {Title with a literal " quote}
units mm
put sphere ell \
  V {0 0 0} A {1 0 0} B {0 1 0} C {0 0 1}
attr set sphere first "line one
line two" second "another
multiline value"
]=])
file(REMOVE "${WORK_DIR}/commands.g")
convert(--input-trait "asc.commands[]=[\"attr\",\"put\",\"title\",\"units\"]"
  "${WORK_DIR}/commands.asc" "${WORK_DIR}/commands.g")
reject_conversion("${WORK_DIR}/contradicted-commands.g" --input-trait "asc.commands[]=[\"title\"]"
  "${WORK_DIR}/commands.asc" "${WORK_DIR}/contradicted-commands.g")
file(WRITE "${WORK_DIR}/unsupported-command.asc" "set variable value\n")
reject_conversion("${WORK_DIR}/unsupported-command.g" "${WORK_DIR}/unsupported-command.asc" "${WORK_DIR}/unsupported-command.g")
file(WRITE "${WORK_DIR}/global-attributes.asc" [=[title initial
units mm
attr set _GLOBAL title {attribute title} units 3.7 regionid_colortable {{100 200 10 20 30}}
units cm
put sphere ell V {0 0 0} A {1 0 0} B {0 1 0} C {0 0 1}
]=])
file(REMOVE "${WORK_DIR}/global-attributes.g" "${WORK_DIR}/global-roundtrip.asc")
convert("${WORK_DIR}/global-attributes.asc" "${WORK_DIR}/global-attributes.g")
convert("${WORK_DIR}/global-attributes.g" "${WORK_DIR}/global-roundtrip.asc")
file(READ "${WORK_DIR}/global-roundtrip.asc" global_roundtrip)
string(FIND "${global_roundtrip}" "title \"attribute title\"" global_title)
string(FIND "${global_roundtrip}" "regionid_colortable" global_colors)
if(global_title LESS 0 OR global_colors LESS 0)
  message(FATAL_ERROR "ASC global attributes did not survive subsequent commands and a round trip")
endif()
reject_conversion("${WORK_DIR}/missing-directory/output.asc" --output-handler "ASC Writer" "${WORK_DIR}/model.g" "${WORK_DIR}/missing-directory/output.asc")
file(WRITE "${WORK_DIR}/incomplete.asc" "title {unterminated title\n")
reject_conversion("${WORK_DIR}/incomplete-asc.g" --input-format application/vnd.brlcad.script "${WORK_DIR}/incomplete.asc" "${WORK_DIR}/incomplete-asc.g")
file(WRITE "${WORK_DIR}/unsupported.asc" "title {test}\nunsupported_command\n")
reject_conversion("${WORK_DIR}/unsupported-asc.g" --input-format application/vnd.brlcad.script "${WORK_DIR}/unsupported.asc" "${WORK_DIR}/unsupported-asc.g")

reject_conversion("${WORK_DIR}/missing-directory/output.ply" --output-handler "PLY Writer" "${WORK_DIR}/model.g" "${WORK_DIR}/missing-directory/output.ply")
file(WRITE "${WORK_DIR}/invalid.ply" "ply\nformat ascii 1.0\nelement vertex 3\nproperty float x\nproperty float y\nproperty float z\nelement face 1\nproperty list uchar int vertex_indices\nend_header\n0 0 0\n1 0 0\n0 1 0\n3 0 1 99\n")
reject_conversion("${WORK_DIR}/invalid-ply.g" --input-handler "PLY Reader" "${WORK_DIR}/invalid.ply" "${WORK_DIR}/invalid-ply.g")
file(READ "${WORK_DIR}/invalid.ply" fractional_ply)
string(REPLACE "uchar int" "uchar double" fractional_ply "${fractional_ply}")
string(REPLACE "3 0 1 99" "3 0 1 1e-100" fractional_ply "${fractional_ply}")
file(WRITE "${WORK_DIR}/fractional.ply" "${fractional_ply}")
reject_conversion("${WORK_DIR}/fractional-ply.g" --input-handler "PLY Reader" "${WORK_DIR}/fractional.ply" "${WORK_DIR}/fractional-ply.g")
file(READ "${SOURCE_DIR}/regress/gcv/ply/tetra.ply" varying_colors)
string(REPLACE "3 0 1 2 255 0 255" "3 0 1 2 0 255 0" varying_colors "${varying_colors}")
file(WRITE "${WORK_DIR}/varying-colors.ply" "${varying_colors}")
file(REMOVE "${WORK_DIR}/varying-colors.g")
convert(--reject-loss geometry --input-handler "PLY Reader" "${WORK_DIR}/varying-colors.ply" "${WORK_DIR}/varying-colors.g")
reject_conversion("${WORK_DIR}/rejected-colors.g" --reject-loss appearance --input-handler "PLY Reader"
  "${WORK_DIR}/varying-colors.ply" "${WORK_DIR}/rejected-colors.g")
file(REMOVE "${WORK_DIR}/export.ply" "${WORK_DIR}/ply-roundtrip.g")
convert("${WORK_DIR}/model.g" "${WORK_DIR}/export.ply")
convert(--input-handler "PLY Reader" "${WORK_DIR}/export.ply" "${WORK_DIR}/ply-roundtrip.g")

if(UNIX AND EXISTS "/dev/full")
  execute_process(COMMAND sh -c "test -c /dev/full" RESULT_VARIABLE full_device)
  if(full_device EQUAL 0)
    foreach(writer "ASC Writer" "PLY Writer")
      if(writer STREQUAL "ASC Writer")
        set(type application/vnd.brlcad.script)
      else()
        set(type application/x-ply)
      endif()
      execute_process(COMMAND "${GCV}" --output-handler "${writer}" --output-format "${type}"
        "${WORK_DIR}/model.g" /dev/full RESULT_VARIABLE status ERROR_VARIABLE messages)
      if(NOT status EQUAL 1)
        message(FATAL_ERROR "${writer} did not return a clean late-write failure: ${status}\n${messages}")
      endif()
    endforeach()
  endif()
endif()

foreach(profile ascii little-endian big-endian)
  set(ply_file "${WORK_DIR}/${profile}.ply")
  set(ply_db "${WORK_DIR}/${profile}-ply.g")
  file(REMOVE "${ply_file}" "${ply_db}")
  if(profile STREQUAL "ascii")
    set(request --output-trait encoding=ascii)
    set(input_request --input-trait encoding=ascii)
    set(signature "format ascii 1.0")
  else()
    set(request --output-trait encoding=binary --output-trait "byte-order=${profile}")
    set(input_request --input-trait encoding=binary --input-trait "byte-order=${profile}")
    string(REPLACE "-" "_" endian "${profile}")
    set(signature "format binary_${endian} 1.0")
  endif()
  convert(--output-handler "PLY Writer" ${request} "${WORK_DIR}/model.g" "${ply_file}")
  file(READ "${ply_file}" header LIMIT 128)
  if(NOT header MATCHES "${signature}")
    message(FATAL_ERROR "PLY output profile was not honored: ${header}")
  endif()
  convert(--input-handler "PLY Reader" "${ply_file}" "${ply_db}")
  reject_conversion("${WORK_DIR}/${profile}-unknown-ply.g" --input-handler "PLY Reader"
    --input-format application/x-ply --input-trait fixture.unestablished=assertion
    "${ply_file}" "${WORK_DIR}/${profile}-unknown-ply.g")
  if(HAS_ASSETIMPORT)
    convert(--input-handler "Assetimport Reader" --input-format application/x-ply --input-trait version=1.0
      ${input_request} "${ply_file}" "${WORK_DIR}/${profile}-sdk-ply.g")
    reject_conversion("${WORK_DIR}/${profile}-unknown-sdk-ply.g" --input-handler "Assetimport Reader"
      --input-format application/x-ply --input-trait fixture.unestablished=assertion
      "${ply_file}" "${WORK_DIR}/${profile}-unknown-sdk-ply.g")
  endif()
endforeach()
foreach(partial version=1.0 encoding=binary)
  file(REMOVE "${WORK_DIR}/partial-be.ply" "${WORK_DIR}/partial-be.g")
  convert(--output-handler "PLY Writer" --output-trait "${partial}"
    -i "${WORK_DIR}/model.g" -o "${WORK_DIR}/partial-be.ply" -O --type be)
  file(READ "${WORK_DIR}/partial-be.ply" header LIMIT 128)
  if(NOT header MATCHES "format binary_big_endian 1[.]0")
    message(FATAL_ERROR "PLY partial request overrode its compatible --type be: ${header}")
  endif()
  convert(--input-handler "PLY Reader" "${WORK_DIR}/partial-be.ply" "${WORK_DIR}/partial-be.g")
endforeach()
file(REMOVE "${WORK_DIR}/native-default.ply" "${WORK_DIR}/native-matching.ply")
convert(--output-handler "PLY Writer" --output-trait version=1.0
  -i "${WORK_DIR}/model.g" -o "${WORK_DIR}/native-default.ply" -O --type de)
file(READ "${WORK_DIR}/native-default.ply" header LIMIT 128)
if(header MATCHES "format binary_big_endian 1[.]0")
  set(native_order big-endian)
  set(other_order little-endian)
elseif(header MATCHES "format binary_little_endian 1[.]0")
  set(native_order little-endian)
  set(other_order big-endian)
else()
  message(FATAL_ERROR "PLY --type de did not select native binary storage: ${header}")
endif()
convert(--output-handler "PLY Writer" --output-trait "byte-order=${native_order}"
  -i "${WORK_DIR}/model.g" -o "${WORK_DIR}/native-matching.ply" -O --type de)
reject_conversion("${WORK_DIR}/native-conflict.ply" --output-handler "PLY Writer"
  --output-trait "byte-order=${other_order}" -i "${WORK_DIR}/model.g" -o "${WORK_DIR}/native-conflict.ply" -O --type de)
reject_conversion("${WORK_DIR}/explicit-order-conflict.ply" --output-handler "PLY Writer"
  --output-trait byte-order=big-endian -i "${WORK_DIR}/model.g" -o "${WORK_DIR}/explicit-order-conflict.ply" -O --type le)
file(REMOVE "${WORK_DIR}/native-option.stl")
convert(--output-handler "STL Writer" --output-trait byte-order=little-endian
  -i "${WORK_DIR}/model.g" -o "${WORK_DIR}/native-option.stl" -O --binary)
execute_process(COMMAND "${GCV}" --identify "${WORK_DIR}/native-option.stl"
  RESULT_VARIABLE native_stl_status ERROR_VARIABLE native_stl_identity)
if(NOT native_stl_status EQUAL 0 OR NOT native_stl_identity MATCHES "encoding=binary")
  message(FATAL_ERROR "STL --binary did not honor its compatible byte-order request: ${native_stl_identity}")
endif()
reject_conversion("${WORK_DIR}/native-option-conflict.stl" --output-handler "STL Writer"
  --output-trait encoding=ascii -i "${WORK_DIR}/model.g" -o "${WORK_DIR}/native-option-conflict.stl" -O --binary)
string(ASCII 195 169 utf8_e_acute)
file(WRITE "${WORK_DIR}/stl-charset.asc"
  "units mm\nput {caf${utf8_e_acute}.s} arb8 V1 {0 0 0} V2 {1 0 0} V3 {1 1 0} V4 {0 1 0} V5 {0 0 1} V6 {1 0 1} V7 {1 1 1} V8 {0 1 1}\nput {caf${utf8_e_acute}.r} comb region yes tree {l {caf${utf8_e_acute}.s}}\n")
convert("${WORK_DIR}/stl-charset.asc" "${WORK_DIR}/stl-charset.g")
set(stl_marker "Preserve the destination when labels cannot satisfy ASCII.\n")
file(WRITE "${WORK_DIR}/stl-charset.stl" "${stl_marker}")
execute_process(COMMAND "${GCV}" --output-handler "STL Writer" --output-trait encoding=ascii
  "${WORK_DIR}/stl-charset.g" "${WORK_DIR}/stl-charset.stl"
  RESULT_VARIABLE stl_charset_status ERROR_VARIABLE stl_charset_messages)
file(READ "${WORK_DIR}/stl-charset.stl" stl_charset_contents)
if(NOT stl_charset_status EQUAL 1 OR NOT stl_charset_contents STREQUAL stl_marker)
  message(FATAL_ERROR "STL ASCII assertion did not preserve the destination: ${stl_charset_status}\n${stl_charset_messages}")
endif()
file(MAKE_DIRECTORY "${WORK_DIR}/stl-charset-dir")
file(WRITE "${WORK_DIR}/stl-charset-dir/kept.txt" "${stl_marker}")
execute_process(COMMAND "${GCV}" --output-handler "STL Writer" --output-format model/stl --output-trait encoding=ascii
  -i "${WORK_DIR}/stl-charset.g" -o "${WORK_DIR}/stl-charset-dir" -O --output-dir
  RESULT_VARIABLE stl_charset_status ERROR_VARIABLE stl_charset_messages)
file(GLOB stl_charset_outputs "${WORK_DIR}/stl-charset-dir/*.stl")
file(READ "${WORK_DIR}/stl-charset-dir/kept.txt" stl_charset_contents)
if(NOT stl_charset_status EQUAL 1 OR stl_charset_outputs OR NOT stl_charset_contents STREQUAL stl_marker)
  message(FATAL_ERROR "STL ASCII assertion opened per-region outputs: ${stl_charset_status}\n${stl_charset_messages}")
endif()
file(REMOVE "${WORK_DIR}/stl-charset.stl")
convert(--output-handler "STL Writer" --output-trait stl.serialization=text
  "${WORK_DIR}/stl-charset.g" "${WORK_DIR}/stl-charset.stl")
file(READ "${WORK_DIR}/stl-charset.stl" stl_charset_contents)
if(NOT stl_charset_contents MATCHES "solid caf${utf8_e_acute}[.]r")
  message(FATAL_ERROR "Unasserted STL text output changed its existing label bytes")
endif()
reject_conversion("${WORK_DIR}/ply-type-conflict.ply" --output-handler "PLY Writer"
  --output-trait encoding=ascii -i "${WORK_DIR}/model.g" -o "${WORK_DIR}/ply-type-conflict.ply" -O --type le)

if(HAS_ASSETIMPORT)
  if(formats MATCHES "application/vnd.americandynamics.acc.*Assetimport Reader")
    message(FATAL_ERROR "AC3D importer advertised an unrelated MIME identity")
  endif()
  file(WRITE "${WORK_DIR}/triangle.acc" "AC3Db\nOBJECT poly\nnumvert 3\n0 0 0\n1 0 0\n0 1 0\nnumsurf 1\nSURF 0x20\nrefs 3\n0 0 0\n1 0 0\n2 0 0\nkids 0\n")
  file(WRITE "${WORK_DIR}/triangle.off" "OFF\n3 1 0\n0 0 0\n1 0 0\n0 1 0\n3 0 1 2\n")
  foreach(format acc off)
    file(REMOVE "${WORK_DIR}/${format}-mapped.g")
    convert(--input-handler "Assetimport Reader" "${WORK_DIR}/triangle.${format}" "${WORK_DIR}/${format}-mapped.g")
  endforeach()
  foreach(profile ascii binary)
    file(REMOVE "${WORK_DIR}/asset-${profile}.stl")
    convert(--output-handler "Assetimport Writer" --output-trait "encoding=${profile}"
      "${WORK_DIR}/model.g" "${WORK_DIR}/asset-${profile}.stl")
    execute_process(COMMAND "${GCV}" --identify "${WORK_DIR}/asset-${profile}.stl"
      RESULT_VARIABLE status ERROR_VARIABLE identified)
    if(NOT status EQUAL 0 OR NOT identified MATCHES "encoding=${profile}")
      message(FATAL_ERROR "SDK variant selection was not honored: ${identified}")
    endif()
  endforeach()
  reject_conversion("${WORK_DIR}/asset-variant-conflict.stl" --output-handler "Assetimport Writer"
    --output-trait encoding=binary -i "${WORK_DIR}/model.g" -o "${WORK_DIR}/asset-variant-conflict.stl" -O --format stl)
endif()

configure_file("${SOURCE_DIR}/regress/gcv/gltf/tetra.bin" "${WORK_DIR}/tetra.bin" COPYONLY)
file(READ "${SOURCE_DIR}/regress/gcv/gltf/tetra-gltf2.0.gltf" gltf_source)
file(WRITE "${WORK_DIR}/profile.gltf" "${gltf_source}")
file(REMOVE "${WORK_DIR}/gltf-profile.g")
convert(--input-handler "GLTF Reader" "${WORK_DIR}/profile.gltf" "${WORK_DIR}/gltf-profile.g")
set(required "[\"KHR_mesh_quantization\",\"KHR_draco_mesh_compression\"]")
string(JSON gltf_required SET "${gltf_source}" extensionsRequired "${required}")
string(JSON gltf_required SET "${gltf_required}" extensionsUsed "${required}")
file(WRITE "${WORK_DIR}/required.gltf" "${gltf_required}")
reject_conversion("${WORK_DIR}/gltf-required.g" --input-handler "GLTF Reader" "${WORK_DIR}/required.gltf" "${WORK_DIR}/gltf-required.g")
string(JSON gltf_future SET "${gltf_source}" asset version "\"3.0\"")
file(WRITE "${WORK_DIR}/future.gltf" "${gltf_future}")
reject_conversion("${WORK_DIR}/gltf-future.g" --input-handler "GLTF Reader" "${WORK_DIR}/future.gltf" "${WORK_DIR}/gltf-future.g")
foreach(vrm_extension VRM VRMC_vrm)
  if(vrm_extension STREQUAL "VRM")
    set(vrm_version "0.0")
  else()
    set(vrm_version "1.0")
  endif()
  string(JSON vrm_source SET "${gltf_source}" extensions "{\"${vrm_extension}\":{\"specVersion\":\"${vrm_version}\"}}")
  string(JSON vrm_source SET "${vrm_source}" extensionsUsed "[\"${vrm_extension}\"]")
  file(WRITE "${WORK_DIR}/${vrm_extension}.gltf" "${vrm_source}")
  file(REMOVE "${WORK_DIR}/${vrm_extension}.g")
  convert(--input-handler "GLTF Reader" --input-trait "vrm.extension=${vrm_extension}"
    --input-trait "vrm.version=${vrm_version}" --input-trait "gltf.used-extensions[]=[\"${vrm_extension}\"]"
    "${WORK_DIR}/${vrm_extension}.gltf" "${WORK_DIR}/${vrm_extension}.g")
  reject_conversion("${WORK_DIR}/${vrm_extension}-wrong.g" --input-handler "GLTF Reader"
    --input-trait vrm.version=99 "${WORK_DIR}/${vrm_extension}.gltf" "${WORK_DIR}/${vrm_extension}-wrong.g")
  string(JSON vrm_required SET "${vrm_source}" extensionsRequired "[\"${vrm_extension}\"]")
  file(WRITE "${WORK_DIR}/${vrm_extension}-required.gltf" "${vrm_required}")
  reject_conversion("${WORK_DIR}/${vrm_extension}-required.g" --input-handler "GLTF Reader"
    "${WORK_DIR}/${vrm_extension}-required.gltf" "${WORK_DIR}/${vrm_extension}-required.g")
endforeach()
reject_conversion("${WORK_DIR}/gltf-unobserved.g" --input-handler "GLTF Reader"
  --input-trait fixture.unestablished=assertion "${WORK_DIR}/profile.gltf" "${WORK_DIR}/gltf-unobserved.g")
string(JSON gltf_minimum SET "${gltf_source}" asset minVersion "\"2.1\"")
file(WRITE "${WORK_DIR}/minimum.gltf" "${gltf_minimum}")
reject_conversion("${WORK_DIR}/gltf-minimum.g" --input-handler "GLTF Reader" "${WORK_DIR}/minimum.gltf" "${WORK_DIR}/gltf-minimum.g")
string(REPEAT "x" 70000 padding)
string(REGEX REPLACE "^[ \t\r\n]*\\{" "" gltf_tail "${gltf_source}")
file(WRITE "${WORK_DIR}/large.gltf" "{\"extras\":{\"padding\":\"${padding}\"},${gltf_tail}")
file(REMOVE "${WORK_DIR}/gltf-large.g")
convert(--input-handler "GLTF Reader" "${WORK_DIR}/large.gltf" "${WORK_DIR}/gltf-large.g")
set(deep_metadata "0")
# Full decoding permits metadata beyond the prefix inspector's depth limit.
set(metadata_levels 65)
foreach(level RANGE 1 ${metadata_levels})
  set(deep_metadata "{\"nested\":${deep_metadata}}")
endforeach()
string(JSON deep_gltf SET "${gltf_source}" extras "${deep_metadata}")
file(WRITE "${WORK_DIR}/deep-metadata.gltf" "${deep_gltf}")
file(REMOVE "${WORK_DIR}/deep-metadata.g")
convert(--input-handler "GLTF Reader" "${WORK_DIR}/deep-metadata.gltf" "${WORK_DIR}/deep-metadata.g")
string(REGEX REPLACE "^[ \t\r\n]*\\{" "" gltf_tail "${gltf_required}")
file(WRITE "${WORK_DIR}/late-required.gltf" "{\"extras\":{\"padding\":\"${padding}\"},${gltf_tail}")
reject_conversion("${WORK_DIR}/gltf-late-required.g" --input-handler "GLTF Reader" "${WORK_DIR}/late-required.gltf" "${WORK_DIR}/gltf-late-required.g")
reject_conversion("${WORK_DIR}/gltf-asserted-required.g" --input-handler "GLTF Reader"
  --input-trait "gltf.required-extensions[]=[]" "${WORK_DIR}/late-required.gltf" "${WORK_DIR}/gltf-asserted-required.g")
reject_conversion("${WORK_DIR}/gltf-identity.g" --input-handler "GLTF Reader" --input-format model/gltf-binary
  "${WORK_DIR}/profile.gltf" "${WORK_DIR}/gltf-identity.g")

if(HAS_ASSETIMPORT AND formats MATCHES "model/gltf[+]json[^\n]*Assetimport Reader" AND
   formats MATCHES "model/gltf-binary[^\n]*Assetimport Writer")
  file(REMOVE "${WORK_DIR}/container.vrm" "${WORK_DIR}/container-vrm.g")
  convert(--output-format model/gltf-binary --output-trait version=2.0 --output-handler "Assetimport Writer"
    "${WORK_DIR}/model.g" "${WORK_DIR}/container.vrm")
  convert(--input-handler "Assetimport Reader" "${WORK_DIR}/container.vrm" "${WORK_DIR}/container-vrm.g")
  execute_process(COMMAND "${GCV}" --identify "${WORK_DIR}/container.vrm"
    RESULT_VARIABLE vrm_status ERROR_VARIABLE vrm_identity)
  if(NOT vrm_status EQUAL 0 OR NOT vrm_identity MATCHES "model/gltf-binary" OR vrm_identity MATCHES "vrm.version=")
    message(FATAL_ERROR "The .vrm suffix alone incorrectly established an avatar profile: ${vrm_identity}")
  endif()
  foreach(gltf_version 1.0 2.0)
    foreach(container json binary)
      if(container STREQUAL "json")
        set(gltf_type model/gltf+json)
        set(gltf_encoding utf-8)
      else()
        set(gltf_type model/gltf-binary)
        set(gltf_encoding binary)
      endif()
      set(gltf_wire "${WORK_DIR}/sdk-gltf-${gltf_version}-${container}.payload")
      set(gltf_native "${WORK_DIR}/sdk-gltf-${gltf_version}-${container}.g")
      file(REMOVE "${gltf_wire}" "${gltf_native}")
      if(gltf_version STREQUAL "1.0" AND container STREQUAL "binary")
        reject_conversion("${gltf_wire}" --output-handler "Assetimport Writer" --output-format "${gltf_type}"
          --output-trait version=1.0 "${WORK_DIR}/model.g" "${gltf_wire}")
        continue()
      endif()
      get_filename_component(gltf_name "${gltf_wire}" NAME)
      # The SDK's glTF 1 reader prepends its directory to absolute buffer URIs.
      execute_process(COMMAND "${GCV}" --output-handler "Assetimport Writer" --output-format "${gltf_type}"
        --output-trait "version=${gltf_version}" --output-trait "encoding=${gltf_encoding}"
        "${WORK_DIR}/model.g" "${gltf_name}" WORKING_DIRECTORY "${WORK_DIR}"
        RESULT_VARIABLE gltf_status ERROR_VARIABLE gltf_messages)
      if(NOT gltf_status EQUAL 0)
        message(FATAL_ERROR "SDK glTF profile export failed: ${gltf_messages}")
      endif()
      execute_process(COMMAND "${GCV}" --identify "${gltf_wire}"
        RESULT_VARIABLE gltf_status ERROR_VARIABLE gltf_identity)
      if(NOT gltf_status EQUAL 0 OR NOT gltf_identity MATCHES "version=${gltf_version}" OR
         NOT gltf_identity MATCHES "encoding=${gltf_encoding}")
        message(FATAL_ERROR "SDK glTF output did not match the selected profile: ${gltf_identity}")
      endif()
      convert(--input-handler "Assetimport Reader" --input-format "${gltf_type}"
        --input-trait "version=${gltf_version}" --input-trait "encoding=${gltf_encoding}"
        "${gltf_wire}" "${gltf_native}")
    endforeach()
  endforeach()
  foreach(vrm_extension VRM VRMC_vrm)
    if(vrm_extension STREQUAL "VRM")
      set(vrm_version 0.0)
    else()
      set(vrm_version 1.0)
    endif()
    file(REMOVE "${WORK_DIR}/sdk-${vrm_extension}.g")
    convert(--input-handler "Assetimport Reader" --input-trait "vrm.version=${vrm_version}"
      "${WORK_DIR}/${vrm_extension}.gltf" "${WORK_DIR}/sdk-${vrm_extension}.g")
    reject_conversion("${WORK_DIR}/sdk-${vrm_extension}-required.g" --input-handler "Assetimport Reader"
      "${WORK_DIR}/${vrm_extension}-required.gltf" "${WORK_DIR}/sdk-${vrm_extension}-required.g")
  endforeach()
  foreach(sample minimum late-required)
    reject_conversion("${WORK_DIR}/sdk-gltf-${sample}.g" --input-handler "Assetimport Reader"
      "${WORK_DIR}/${sample}.gltf" "${WORK_DIR}/sdk-gltf-${sample}.g")
  endforeach()
  reject_conversion("${WORK_DIR}/sdk-gltf-unobserved.g" --input-handler "Assetimport Reader"
    --input-trait fixture.unestablished=assertion "${WORK_DIR}/profile.gltf" "${WORK_DIR}/sdk-gltf-unobserved.g")
  string(REGEX REPLACE "^[ \t\r\n]*\\{" "" avatar_tail "${vrm_source}")
  file(WRITE "${WORK_DIR}/late-avatar.gltf" "{\"extras\":{\"padding\":\"${padding}\"},${avatar_tail}")
  file(REMOVE "${WORK_DIR}/sdk-gltf-late-avatar.g")
  convert(--input-handler "Assetimport Reader" --input-trait vrm.version=1.0
    "${WORK_DIR}/late-avatar.gltf" "${WORK_DIR}/sdk-gltf-late-avatar.g")
  reject_conversion("${WORK_DIR}/sdk-gltf-wrong-avatar.g" --input-handler "Assetimport Reader"
    --input-trait vrm.version=99 "${WORK_DIR}/late-avatar.gltf" "${WORK_DIR}/sdk-gltf-wrong-avatar.g")
endif()

file(WRITE "${WORK_DIR}/partial.scad" "cube(1); unsupported_shape();\n")
file(REMOVE "${WORK_DIR}/partial-scad.g")
execute_process(COMMAND "${GCV}" --input-handler "OpenSCAD Reader"
  "${WORK_DIR}/partial.scad" "${WORK_DIR}/partial-scad.g" RESULT_VARIABLE status ERROR_VARIABLE messages)
if(NOT status EQUAL 0 OR NOT EXISTS "${WORK_DIR}/partial-scad.g" OR NOT messages MATCHES "Partial conversion" OR NOT messages MATCHES "openscad.unsupported-constructs")
  message(FATAL_ERROR "OpenSCAD omissions were not reported as partial: ${messages}")
endif()
reject_conversion("${WORK_DIR}/strict-scad.g" --reject-loss geometry --input-handler "OpenSCAD Reader"
  "${WORK_DIR}/partial.scad" "${WORK_DIR}/strict-scad.g")
file(WRITE "${WORK_DIR}/aborted.scad" "cube(1); assert(false, \"failed evaluation\");\n")
reject_conversion("${WORK_DIR}/aborted-scad.g" --input-handler "OpenSCAD Reader"
  "${WORK_DIR}/aborted.scad" "${WORK_DIR}/aborted-scad.g")
file(WRITE "${WORK_DIR}/asserted.scad" "assert(true) cube(1);\n")
file(REMOVE "${WORK_DIR}/asserted-scad.g")
convert(--reject-loss all --input-handler "OpenSCAD Reader" "${WORK_DIR}/asserted.scad" "${WORK_DIR}/asserted-scad.g")
