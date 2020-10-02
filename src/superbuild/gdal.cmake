set(gdal_DESCRIPTION "
Option for enabling and disabling compilation of the GDAL geographic
library provided with BRL-CAD's source code.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(gdal GDAL gdal gdal_DESCRIPTION REQUIRED_VARS "BRLCAD_ENABLE_GDAL;BRLCAD_LEVEL2" ALIASES ENABLE_GDAL)

if (${CMAKE_PROJECT_NAME}_GDAL_BUILD)

  if (MSVC)
    set(GDAL_BASENAME gdal)
  else (MSVC)
    set(GDAL_BASENAME libgdal)
  endif (MSVC)

  set(TARGET_LIST ZLIB PNG PROJ4)
  foreach(T ${TARGET_LIST})
    if (TARGET ${T}_BLD)
      set(${T}_TARGET ${T}_BLD)
    endif (TARGET ${T}_BLD)
  endforeach(T ${TARGET_LIST})

  ExternalProject_Add(GDAL_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gdal"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    -DZLIB_ROOT=${CMAKE_BINARY_DIR} -DPNG_ROOT=${CMAKE_BINARY_DIR} -DPROJ4_ROOT=${CMAKE_BINARY_DIR}
    -DGDAL_INST_DATA_DIR=${CMAKE_INSTALL_PREFIX}/${DATA_DIR}/gdal
    DEPENDS ${PROJ4_TARGET} ${PNG_TARGET} ${ZLIB_TARGET}
    )
  ExternalProject_Target(gdal GDAL_BLD
    OUTPUT_FILE ${GDAL_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE ${GDAL_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    RPATH
    )

  set(GDAL_EXECUTABLES gdalinfo gdallocationinfo gdal_translate gdaltransform gdaldem gdalwarp gdalbuildvrt)
  foreach(GDALEXEC ${GDAL_EXECUTABLES})
    ExternalProject_Target(${GDALEXEC} GDAL_BLD
      OUTPUT_FILE ${GDALEXEC}${CMAKE_EXECUTABLE_SUFFIX}
      RPATH EXEC
      )
  endforeach(GDALEXEC ${GDAL_EXECUTABLES})

  ExternalProject_ByProducts(GDAL_BLD ${DATA_DIR}/gdal
    LICENSE.TXT
    GDALLogoBW.svg
    GDALLogoColor.svg
    GDALLogoGS.svg
    compdcs.csv
    coordinate_axis.csv
    cubewerx_extra.wkt
    datum_shift.csv
    default.rsc
    ecw_cs.wkt
    ellipsoid.csv
    epsg.wkt
    esri_StatePlane_extra.wkt
    esri_Wisconsin_extra.wkt
    esri_extra.wkt
    gcs.csv
    gcs.override.csv
    gdal_datum.csv
    gdalicon.png
    gdalvrt.xsd
    geoccs.csv
    gml_registry.xml
    gmlasconf.xml
    gmlasconf.xsd
    gt_datum.csv
    gt_ellips.csv
    header.dxf
    inspire_cp_BasicPropertyUnit.gfs
    inspire_cp_CadastralBoundary.gfs
    inspire_cp_CadastralParcel.gfs
    inspire_cp_CadastralZoning.gfs
    jpfgdgml_AdmArea.gfs
    jpfgdgml_AdmBdry.gfs
    jpfgdgml_AdmPt.gfs
    jpfgdgml_BldA.gfs
    jpfgdgml_BldL.gfs
    jpfgdgml_Cntr.gfs
    jpfgdgml_CommBdry.gfs
    jpfgdgml_CommPt.gfs
    jpfgdgml_Cstline.gfs
    jpfgdgml_ElevPt.gfs
    jpfgdgml_GCP.gfs
    jpfgdgml_LeveeEdge.gfs
    jpfgdgml_RailCL.gfs
    jpfgdgml_RdASL.gfs
    jpfgdgml_RdArea.gfs
    jpfgdgml_RdCompt.gfs
    jpfgdgml_RdEdg.gfs
    jpfgdgml_RdMgtBdry.gfs
    jpfgdgml_RdSgmtA.gfs
    jpfgdgml_RvrMgtBdry.gfs
    jpfgdgml_SBAPt.gfs
    jpfgdgml_SBArea.gfs
    jpfgdgml_SBBdry.gfs
    jpfgdgml_WA.gfs
    jpfgdgml_WL.gfs
    jpfgdgml_WStrA.gfs
    jpfgdgml_WStrL.gfs
    netcdf_config.xsd
    nitf_spec.xml
    nitf_spec.xsd
    ogrvrt.xsd
    osmconf.ini
    ozi_datum.csv
    ozi_ellips.csv
    pci_datum.txt
    pci_ellips.txt
    pcs.csv
    pcs.override.csv
    plscenesconf.json
    prime_meridian.csv
    projop_wparm.csv
    ruian_vf_ob_v1.gfs
    ruian_vf_st_uvoh_v1.gfs
    ruian_vf_st_v1.gfs
    ruian_vf_v1.gfs
    s57agencies.csv
    s57attributes.csv
    s57expectedinput.csv
    s57objectclasses.csv
    seed_2d.dgn
    seed_3d.dgn
    stateplane.csv
    trailer.dxf
    unit_of_measure.csv
    vdv452.xml
    vdv452.xsd
    vertcs.csv
    vertcs.override.csv
    )

  list(APPEND BRLCAD_DEPS GDAL_BLD)

  set(GDAL_LIBRARIES gdal CACHE STRING "Building bundled gdal" FORCE)
  set(GDAL_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/gdal" CACHE STRING "Directory containing GDAL headers." FORCE)

  SetTargetFolder(GDAL_BLD "Third Party Libraries")
  SetTargetFolder(gdal "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_GDAL_BUILD)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

