if(SH_EXEC AND TARGET mged AND TARGET asc2g)
  brlcad_add_test(NAME regress-iges COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/iges.sh" ${CMAKE_SOURCE_DIR})
  brlcad_regression_test(regress-iges "iges-g;g-iges;mged;asc2g" TEST_DEFINED)
endif(SH_EXEC AND TARGET mged AND TARGET asc2g)

if(TARGET g-iges AND TARGET iges-g AND TARGET asc2g AND TARGET mged AND TARGET test_iges_rays AND TARGET test_iges_brep)
  foreach(case aliases write_failure multi_file representations drawing_default)
    brlcad_add_test(NAME iges_io_${case}
      COMMAND ${CMAKE_COMMAND}
        "-DGIGES=$<TARGET_FILE:g-iges>" "-DIGESG=$<TARGET_FILE:iges-g>"
        "-DMGED=$<TARGET_FILE:mged>" "-DRAY_CHECK=$<TARGET_FILE:test_iges_rays>"
        "-DCASE=${case}" "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/io-${case}"
        -P "${CMAKE_SOURCE_DIR}/src/conv/iges/tests/io.cmake")
  endforeach()
  foreach(case metadata inward_shell submodel)
    brlcad_add_test(NAME iges_export_failure_${case}
      COMMAND ${CMAKE_COMMAND}
        "-DGIGES=$<TARGET_FILE:g-iges>" "-DIGESG=$<TARGET_FILE:iges-g>"
        "-DMGED=$<TARGET_FILE:mged>" "-DRAY_CHECK=$<TARGET_FILE:test_iges_rays>"
        "-DCASE=${case}" "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/export-failure-${case}"
        -P "${CMAKE_SOURCE_DIR}/src/conv/iges/tests/export_failure.cmake")
  endforeach()
  foreach(case wedge revolve extrude)
    brlcad_add_test(NAME iges_native_${case}
      COMMAND ${CMAKE_COMMAND}
        "-DIGESG=$<TARGET_FILE:iges-g>" "-DGIGES=$<TARGET_FILE:g-iges>"
        "-DMGED=$<TARGET_FILE:mged>" "-DRAY_CHECK=$<TARGET_FILE:test_iges_rays>"
        "-DCASE=${case}" "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/native-${case}"
        "-DFIXTURE=${CMAKE_SOURCE_DIR}/src/conv/iges/tests/native-${case}.igs"
        -P "${CMAKE_SOURCE_DIR}/src/conv/iges/tests/native.cmake")
  endforeach()
  foreach(case IN ITEMS
      "block,primitives,rpp.r,150" "cylinder,primitives,rcc.r,154"
      "cone,primitives,trc.r,156" "sphere,primitives,sph.r,158|168"
      "ellipsoid,primitives,ell.r,168" "torus,primitives,tor.r,160"
      "pipe,primitives,pipe.r,128" "extrusion,primitives,extrude.r,128"
      "cline,primitives,cline.r,128" "elliptical_cylinder,primitives,rec.r,128"
      "pipe_bend,primitives,pipe.r,128" "extrusion_hole,primitives,extrude.r,128"
      "bspline,primitives,legacy.s,128"
      "booleans,boolean-ops,a-b+c,180" "instances,cube,all.g,430"
      "long_names,bearing,bearing,180")
    string(REPLACE "," ";" fields "${case}")
    list(GET fields 0 label)
    list(GET fields 1 model)
    list(GET fields 2 object)
    list(GET fields 3 type)
    brlcad_add_test(NAME iges_roundtrip_${label}
      COMMAND ${CMAKE_COMMAND}
        "-DASC2G=$<TARGET_FILE:asc2g>" "-DGIGES=$<TARGET_FILE:g-iges>"
        "-DIGESG=$<TARGET_FILE:iges-g>" "-DRAY_CHECK=$<TARGET_FILE:test_iges_rays>"
        "-DBREP_CHECK=$<TARGET_FILE:test_iges_brep>"
        "-DMGED=$<TARGET_FILE:mged>" "-DCASE=${label}"
        "-DMODEL=${CMAKE_SOURCE_DIR}/db/${model}.asc" "-DOBJECT=${object}"
        "-DENTITY_TYPE=${type}" "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/roundtrip-${label}"
        -P "${CMAKE_SOURCE_DIR}/src/conv/iges/tests/roundtrip.cmake")
  endforeach()
endif()

cmakefiles(
  m35.asc.gz
  iges.sh
)

# list of temporary files
set(
  iges_outfiles
  iges.brep.export.iges
  iges.brep.flat.g
  iges.brep.flat.iges
  iges.brep.g
  iges.brep.mesh.g
  iges.brep.nmg.g
  iges.brep.plate-default.g
  iges.brep.plate-source.g
  iges.brep.plate-thick.g
  iges.brep.plate-thick.json
  iges.brep.plate.iges
  iges.brep.roundtrip.g
  iges.export.iges
  iges.export.stdout.iges
  iges.export.stdout.norm.iges
  iges.g
  iges.import.export.iges
  iges.import.g
  iges.import2.export.iges
  iges.import2.g
  iges.import3.export.iges
  iges.import3.g
  iges.log
  iges.failed.json
  iges.alias-hard.iges
  iges.alias-symbolic.iges
  iges.native-csg.g
  iges.native-csg.json
  iges.mixed-assembly.g
  iges.mixed-assembly.json
  iges.name-collision.g
  iges.name-collision.json
  iges.mixed-subfigure.g
  iges.mixed-subfigure.json
  iges.bad-property.g
  iges.bad-property.json
  iges.marked.g
  iges.hollerith-boundary.g
  iges.hollerith-boundary.json
  iges.partial.iges
  iges.partial.g
  iges.numeric-boundary.g
  iges.numeric-boundary.json
  iges.mixed-n.g
  iges.mixed-n.json
  iges.mixed-t.g
  iges.mixed-t.json
  iges.missing.iges
  iges.missing.g
  iges.native-n.g
  iges.native-t.g
  iges.native-m.g
  iges.native-m.json
  iges.native-p.g
  iges.native-p.json
  iges.protected.g
  iges.protected.iges
  iges.m35.asc
  iges.m35.g
  iges.m35.r516.iges
)

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${iges_outfiles}")
distclean(${iges_outfiles})

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
