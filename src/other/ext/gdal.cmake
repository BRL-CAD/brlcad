set(gdal_DESCRIPTION "
Option for enabling and disabling compilation of the GDAL geographic
library provided with BRL-CAD's source code.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(gdal GDAL gdal
  gdal_DESCRIPTION
  REQUIRED_VARS "BRLCAD_ENABLE_GDAL;BRLCAD_LEVEL2"
  ALIASES ENABLE_GDAL
  RESET_VARS GDAL_LIBRARY GDAL_LIBRARIES GDAL_INCLUDE_DIR GDAL_INCLUDE_DIRS 
  )

if (BRLCAD_GDAL_BUILD)

  set(GDAL_MAJOR_VERSION 3)
  set(GDAL_MINOR_VERSION 5)
  set(GDAL_PATCH_VERSION 1)
  set(GDAL_API_VERSION 31)
  set(GDAL_VERSION ${GDAL_API_VERSION}.${GDAL_MAJOR_VERSION}.${GDAL_MINOR_VERSION}.${GDAL_PATCH_VERSION})

  set(GDAL_DEPS)
  set(TARGET_LIST zlib png proj)
  foreach(T ${TARGET_LIST})
    if (TARGET ${T}_stage)
      list(APPEND GDAL_DEPS ${T}_stage)
    endif (TARGET ${T}_stage)
  endforeach(T ${TARGET_LIST})

  if (TARGET ZLIB_BLD)
    set(ZLIB_TARGET ZLIB_BLD)
  endif (TARGET ZLIB_BLD)

  if (TARGET PNG_BLD)
    set(PNG_TARGET PNG_BLD)
  endif (TARGET PNG_BLD)

  if (TARGET PROJ_BLD)
    set(PROJ_TARGET PROJ_BLD)
  endif (TARGET PROJ_BLD)

  if (MSVC)
    set(ZLIB_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${ZLIB_BASENAME}.lib)
    set(PNG_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${PNG_BASENAME}.lib)
    set(PROJ_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${PROJ_BASENAME}.lib)
  elseif (OPENBSD)
    set(ZLIB_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.1.2)
    set(PNG_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${PNG_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.1.6)
    set(PROJ_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${PROJ_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.25.9.0.1)
  else (MSVC)
    set(ZLIB_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(PNG_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${PNG_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(PROJ_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${PROJ_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
  endif (MSVC)

  message("PROJ_LIBRARY: ${PROJ_LIBRARY}")

  if (MSVC)
    set(GDAL_BASENAME gdal)
    set(GDAL_STATICNAME gdal-static)
    set(GDAL_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(GDAL_SYMLINK_1 ${GDAL_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(GDAL_SYMLINK_2 ${GDAL_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.${GDAL_API_VERSION})
  elseif (APPLE)
    set(GDAL_BASENAME libgdal)
    set(GDAL_SUFFIX .${GDAL_API_VERSION}.0.1${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(GDAL_SYMLINK_1 ${GDAL_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(GDAL_SYMLINK_2 ${GDAL_BASENAME}.${GDAL_API_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX})
  elseif (OPENBSD)
    set(GDAL_BASENAME libgdal)
    set(GDAL_STATICNAME libgdal)
    set(GDAL_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${PROJ_API_VERSION}.0)
  else (MSVC)
    set(GDAL_BASENAME libgdal)
    set(GDAL_STATICNAME libgdal)
    set(GDAL_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${GDAL_API_VERSION}.0.1)
    set(GDAL_SYMLINK_1 ${GDAL_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(GDAL_SYMLINK_2 ${GDAL_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.${GDAL_API_VERSION})
  endif (MSVC)

  set(GDAL_INSTDIR ${CMAKE_BINARY_INSTALL_ROOT}/gdal)


  # This option list needs to get a LOT more extensive - see
  # https://gdal.org/build_hints.html for more details.  We'll
  # probably need to disable the optional packages by default
  # and build up as we figure out which ones can be turned on
  # without requiring additional dependencies
  ExternalProject_Add(GDAL_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gdal"
    BUILD_ALWAYS ${EXT_BUILD_ALWAYS} ${LOG_OPTS}
    CMAKE_ARGS
    $<$<NOT:$<BOOL:${CMAKE_CONFIGURATION_TYPES}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
    $<$<BOOL:${ZLIB_TARGET}>:-DZ_PREFIX=ON>
    $<$<BOOL:${ZLIB_TARGET}>:-DZ_PREFIX_STR=${Z_PREFIX_STR}>
    -DBIN_DIR=${BIN_DIR}
    -DLIB_DIR=${LIB_DIR}
    -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_INSTALL_PREFIX=${GDAL_INSTDIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
    -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=${CMAKE_INSTALL_RPATH_USE_LINK_PATH}
    -DCMAKE_SKIP_BUILD_RPATH=${CMAKE_SKIP_BUILD_RPATH}
    -DGDAL_INST_DATA_DIR=${CMAKE_INSTALL_PREFIX}/${DATA_DIR}/gdal
    -DGDAL_USE_EXTERNAL_LIBS=OFF
    -DGDAL_USE_INTERNAL_LIBS=ON # GEOTIFF, GIF, JPEG, JSONC, LERC, OPENCAD, PNG, QHULL, TIFF, ZLIB
    -DGDAL_USE_ZLIB_INTERNAL=OFF
    -DGDAL_USE_ZLIB=ON
    -DZLIB_ROOT=${CMAKE_BINARY_ROOT}
    -DZLIB_LIBRARY=$<$<BOOL:${ZLIB_TARGET}>:${ZLIB_LIBRARY}>
    -DGDAL_USE_PNG_INTERNAL=OFF
    -DGDAL_USE_PNG=ON
    -DPNG_ROOT=${CMAKE_BINARY_ROOT}
    -DPNG_LIBRARY=$<$<BOOL:${PNG_TARGET}>:${PNG_LIBRARY}>
    -DGDAL_USE_PROJ_INTERNAL=OFF
    -DGDAL_USE_PROJ=ON
    -DPROJ_ROOT=${CMAKE_BINARY_ROOT}
    -DPROJ_LIBRARY=$<$<BOOL:${PROJ_TARGET}>:${PROJ_LIBRARY}>
    -DPROJ_LIBRARY_RELEASE=$<$<BOOL:${PROJ_TARGET}>:${PROJ_LIBRARY}>
    -DPROJ_LIBRARY_DEBUG=$<$<BOOL:${PROJ_TARGET}>:${PROJ_LIBRARY}>
    -DGDAL_BUILD_OPTIONAL_DRIVERS=ON
    -DGDAL_ENABLE_DRIVER_BAG=OFF                # libhdf5
    -DGDAL_ENABLE_DRIVER_BASISU=OFF             # basis universal
    -DGDAL_ENABLE_DRIVER_DAAS=OFF               # libcurl
    -DGDAL_ENABLE_DRIVER_DDS=OFF                # crunch lib
    -DGDAL_ENABLE_DRIVER_ECW=OFF                # ECW SDK
    -DGDAL_ENABLE_DRIVER_EEDAI=OFF              # libcurl
    -DGDAL_ENABLE_DRIVER_EXR=OFF                # libopenexr
    -DGDAL_ENABLE_DRIVER_FITS=OFF               # libcfitsio
    -DGDAL_ENABLE_DRIVER_GEOR=OFF               # oracle client libraries
    -DGDAL_ENABLE_DRIVER_GPKG=OFF               # libsqlite3
    -DGDAL_ENABLE_DRIVER_GRASS=OFF              # gdal-grass-driver
    -DGDAL_ENABLE_DRIVER_GTA=OFF                # libgta
    -DGDAL_ENABLE_DRIVER_HDF4=OFF               # libdf
    -DGDAL_ENABLE_DRIVER_HDF5=OFF               # libdf5
    -DGDAL_ENABLE_DRIVER_HEIF=OFF               # libheif
    -DGDAL_ENABLE_DRIVER_JP2ECW=OFF             # ECW SDK
    -DGDAL_ENABLE_DRIVER_JP2KAK=OFF             # kakadu library
    -DGDAL_ENABLE_DRIVER_JP2LURA=OFF            # lurawave library
    -DGDAL_ENABLE_DRIVER_JP2MRSID=OFF           # MrSID SDK
    -DGDAL_ENABLE_DRIVER_JP2OPENJPEG=OFF        # openjpeg >= 2.1
    -DGDAL_ENABLE_DRIVER_JPEGXL=OFF             # libjxl
    -DGDAL_ENABLE_DRIVER_JPIPKAK=OFF            # kakadu library
    -DGDAL_ENABLE_DRIVER_KEA=OFF                # libkea, libhdf5
    -DGDAL_ENABLE_DRIVER_KTX2=OFF               # basis universal
    -DGDAL_ENABLE_DRIVER_MBTILES=OFF            # libsqlite3
    -DGDAL_ENABLE_DRIVER_MRSID=OFF              # MrSID SDK
    -DGDAL_ENABLE_DRIVER_MSG=OFF                # msg library
    -DGDAL_ENABLE_DRIVER_NETCDF=OFF             # libnetcdf
    -DGDAL_ENABLE_DRIVER_NGW=OFF                # libcurl
    -DGDAL_ENABLE_DRIVER_OGCAPI=OFF             # libcurl
    -DGDAL_ENABLE_DRIVER_PDF=OFF                # Poppler/PoDoFo/PDFium (for read support)
    -DGDAL_ENABLE_DRIVER_PLMOSAIC=OFF           # libcurl
    -DGDAL_ENABLE_DRIVER_POSTGISRASTER=OFF      # PostgreSQL library
    -DGDAL_ENABLE_DRIVER_RASDAMAN=OFF           # raslib
    -DGDAL_ENABLE_DRIVER_RASTERLITE=OFF         # libsqlite3
    -DGDAL_ENABLE_DRIVER_SQLITE=OFF             # libsqlite3, librasterlite2, libspatialite
    -DGDAL_ENABLE_DRIVER_RDB=OFF                # rdblib >= 2.2.0
    -DGDAL_ENABLE_DRIVER_TILEDB=OFF             # TileDB
    -DGDAL_ENABLE_DRIVER_WCS=OFF                # libcurl
    -DGDAL_ENABLE_DRIVER_WEBP=OFF               # libwebp
    -DGDAL_ENABLE_DRIVER_WMS=OFF                # libcurl
    -DGDAL_ENABLE_DRIVER_WMTS=OFF               # libcurl
    -DOGR_BUILD_OPTIONAL_DRIVERS=OFF
    -DBUILD_PYTHON_BINDINGS=OFF
    DEPENDS ${GDAL_DEPS}
    LOG_CONFIGURE ${EXT_BUILD_QUIET}
    LOG_BUILD ${EXT_BUILD_QUIET}
    LOG_INSTALL ${EXT_BUILD_QUIET}
    LOG_OUTPUT_ON_FAILURE ${EXT_BUILD_QUIET}
    )

  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/GDAL_BLD-prefix")

  # Tell the parent build about files and libraries
  ExternalProject_Target(SHARED gdal GDAL_BLD ${GDAL_INSTDIR}
    ${GDAL_BASENAME}${GDAL_SUFFIX}
    SYMLINKS ${GDAL_SYMLINK_1};${GDAL_SYMLINK_2}
    LINK_TARGET ${GDAL_SYMLINK_1}
    RPATH
    )
  if (BUILD_STATIC_LIBS)
	  #ExternalProject_Target(STATIC gdal-static GDAL_BLD ${GDAL_INSTDIR}
	  #${GDAL_STATICNAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
	  #)
  endif (BUILD_STATIC_LIBS)

  set(GDAL_EXECUTABLES gdalinfo gdallocationinfo gdal_translate gdaltransform gdaldem gdalwarp gdalbuildvrt)
  foreach(GDALEXEC ${GDAL_EXECUTABLES})
    ExternalProject_Target(EXEC ${GDALEXEC}_exe GDAL_BLD ${GDAL_INSTDIR}
      ${GDALEXEC}${CMAKE_EXECUTABLE_SUFFIX}
      RPATH
      )
  endforeach(GDALEXEC ${GDAL_EXECUTABLES})
  ExternalProject_ByProducts(gdal GDAL_BLD ${GDAL_INSTDIR} ${DATA_DIR}/gdal
	  GDALLogoBW.svg
	  GDALLogoColor.svg
	  GDALLogoGS.svg
	  LICENSE.TXT
	  bag_template.xml
	  cubewerx_extra.wkt
	  default.rsc
	  ecw_cs.wkt
	  eedaconf.json
	  epsg.wkt
	  esri_StatePlane_extra.wkt
	  gdalicon.png
	  gdalmdiminfo_output.schema.json
	  gdalvrt.xsd
	  gml_registry.xml
	  gmlasconf.xml
	  gmlasconf.xsd
	  grib2_center.csv
	  grib2_process.csv
	  grib2_subcenter.csv
	  grib2_table_4_2_0_0.csv
	  grib2_table_4_2_0_1.csv
	  grib2_table_4_2_0_13.csv
	  grib2_table_4_2_0_14.csv
	  grib2_table_4_2_0_15.csv
	  grib2_table_4_2_0_16.csv
	  grib2_table_4_2_0_17.csv
	  grib2_table_4_2_0_18.csv
	  grib2_table_4_2_0_19.csv
	  grib2_table_4_2_0_190.csv
	  grib2_table_4_2_0_191.csv
	  grib2_table_4_2_0_2.csv
	  grib2_table_4_2_0_20.csv
	  grib2_table_4_2_0_3.csv
	  grib2_table_4_2_0_4.csv
	  grib2_table_4_2_0_5.csv
	  grib2_table_4_2_0_6.csv
	  grib2_table_4_2_0_7.csv
	  grib2_table_4_2_10_0.csv
	  grib2_table_4_2_10_1.csv
	  grib2_table_4_2_10_191.csv
	  grib2_table_4_2_10_2.csv
	  grib2_table_4_2_10_3.csv
	  grib2_table_4_2_10_4.csv
	  grib2_table_4_2_1_0.csv
	  grib2_table_4_2_1_1.csv
	  grib2_table_4_2_1_2.csv
	  grib2_table_4_2_20_0.csv
	  grib2_table_4_2_20_1.csv
	  grib2_table_4_2_20_2.csv
	  grib2_table_4_2_2_0.csv
	  grib2_table_4_2_2_3.csv
	  grib2_table_4_2_2_4.csv
	  grib2_table_4_2_2_5.csv
	  grib2_table_4_2_3_0.csv
	  grib2_table_4_2_3_1.csv
	  grib2_table_4_2_3_2.csv
	  grib2_table_4_2_3_3.csv
	  grib2_table_4_2_3_4.csv
	  grib2_table_4_2_3_5.csv
	  grib2_table_4_2_3_6.csv
	  grib2_table_4_2_4_0.csv
	  grib2_table_4_2_4_1.csv
	  grib2_table_4_2_4_10.csv
	  grib2_table_4_2_4_2.csv
	  grib2_table_4_2_4_3.csv
	  grib2_table_4_2_4_4.csv
	  grib2_table_4_2_4_5.csv
	  grib2_table_4_2_4_6.csv
	  grib2_table_4_2_4_7.csv
	  grib2_table_4_2_4_8.csv
	  grib2_table_4_2_4_9.csv
	  grib2_table_4_2_local_Canada.csv
	  grib2_table_4_2_local_HPC.csv
	  grib2_table_4_2_local_MRMS.csv
	  grib2_table_4_2_local_NCEP.csv
	  grib2_table_4_2_local_NDFD.csv
	  grib2_table_4_2_local_index.csv
	  grib2_table_4_5.csv
	  grib2_table_versions.csv
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
	  pdfcomposition.xsd
	  pds4_template.xml
	  plscenesconf.json
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
	  template_tiles.mapml
	  tms_LINZAntarticaMapTileGrid.json
	  tms_MapML_APSTILE.json
	  tms_MapML_CBMTILE.json
	  tms_NZTM2000.json
	  trailer.dxf
	  vdv452.xml
	  vdv452.xsd
	  vicar.json
	  )

  ExternalProject_ByProducts(gdal GDAL_BLD ${GDAL_INSTDIR} ${INCLUDE_DIR}
	  NOINSTALL
	  cpl_atomic_ops.h
	  cpl_auto_close.h
	  cpl_compressor.h
	  cpl_config_extras.h
	  cpl_config.h
	  cpl_conv.h
	  cpl_csv.h
	  cpl_error.h
	  cpl_hash_set.h
	  cpl_http.h
	  cpl_json.h
	  cplkeywordparser.h
	  cpl_list.h
	  cpl_minixml.h
	  cpl_minizip_ioapi.h
	  cpl_minizip_unzip.h
	  cpl_minizip_zip.h
	  cpl_multiproc.h
	  cpl_port.h
	  cpl_progress.h
	  cpl_quad_tree.h
	  cpl_spawn.h
	  cpl_string.h
	  cpl_time.h
	  cpl_virtualmem.h
	  cpl_vsi_error.h
	  cpl_vsi.h
	  cpl_vsi_virtual.h
	  gdal_alg.h
	  gdal_alg_priv.h
	  gdalcachedpixelaccessor.h
	  gdal_csv.h
	  gdal_frmts.h
	  gdalgeorefpamdataset.h
	  gdalgrid.h
	  gdalgrid_priv.h
	  gdal.h
	  gdaljp2abstractdataset.h
	  gdaljp2metadata.h
	  gdal_mdreader.h
	  gdal_pam.h
	  gdalpansharpen.h
	  gdal_priv.h
	  gdal_proxy.h
	  gdal_rat.h
	  gdal_simplesurf.h
	  gdal_utils.h
	  gdal_version.h
	  gdal_vrt.h
	  gdalwarper.h
	  gnm_api.h
	  gnmgraph.h
	  gnm.h
	  memdataset.h
	  ogr_api.h
	  ogr_core.h
	  ogr_feature.h
	  ogr_featurestyle.h
	  ogr_geocoding.h
	  ogr_geometry.h
	  ogr_p.h
	  ogrsf_frmts.h
	  ogr_spatialref.h
	  ogr_srs_api.h
	  ogr_swq.h
	  rawdataset.h
	  vrtdataset.h
    )
  set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} gdal CACHE STRING "Bundled system include dirs" FORCE)

  set(GDAL_LIBRARY gdal CACHE STRING "Building bundled gdal" FORCE)
  set(GDAL_LIBRARIES gdal CACHE STRING "Building bundled gdal" FORCE)

  set(GDAL_INCLUDE_DIR "${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/gdal" CACHE STRING "Directories containing GDAL headers." FORCE)
  set(GDAL_INCLUDE_DIRS "${GDAL_INCLUDE_DIR}" CACHE STRING "Directories containing GDAL headers." FORCE)

  SetTargetFolder(GDAL_BLD "Third Party Libraries")
  SetTargetFolder(gdal "Third Party Libraries")

else (BRLCAD_GDAL_BUILD)

  set(GDAL_LIBRARIES ${GDAL_LIBRARY} CACHE STRING "gdal" FORCE)
  set(GDAL_INCLUDE_DIRS "${GDAL_INCLUDE_DIR}" CACHE STRING "Directories containing GDAL headers." FORCE)

endif (BRLCAD_GDAL_BUILD)

include("${CMAKE_CURRENT_SOURCE_DIR}/gdal.dist")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8


