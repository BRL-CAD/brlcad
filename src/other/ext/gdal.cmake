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

  VERSIONS("${CMAKE_CURRENT_SOURCE_DIR}/gdal/gcore/gdal_version.h.in" GDAL_MAJOR_VERSION GDAL_MINOR_VERSION GDAL_PATCH_VERSION)
  set(GDAL_API_VERSION 32)
  set(GDAL_VERSION ${GDAL_API_VERSION}.${GDAL_MAJOR_VERSION}.${GDAL_MINOR_VERSION}.${GDAL_PATCH_VERSION})
  set(GDAL_DEPS)
  set(TARGET_LIST zlib png proj sqlite3)
  foreach(T ${TARGET_LIST})
    if (TARGET ${T}_stage)
      list(APPEND GDAL_DEPS ${T}_stage)
    endif (TARGET ${T}_stage)
  endforeach(T ${TARGET_LIST})

  if (TARGET ZLIB_BLD)
    set(ZLIB_TARGET ZLIB_BLD)
    if (MSVC)
      set(ZLIB_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${ZLIB_BASENAME}.lib)
    elseif (OPENBSD)
      set(ZLIB_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.1.2)
    else (MSVC)
      set(ZLIB_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    endif (MSVC)
  endif (TARGET ZLIB_BLD)

  if (TARGET PNG_BLD)
    set(PNG_TARGET PNG_BLD)
    if (MSVC)
      set(PNG_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${PNG_BASENAME}.lib)
    elseif (OPENBSD)
      set(PNG_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${PNG_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.1.6)
    else (MSVC)
      set(PNG_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${PNG_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    endif (MSVC)
  endif (TARGET PNG_BLD)

  if (TARGET PROJ_BLD)
    set(PROJ_TARGET PROJ_BLD)
    if (MSVC)
      set(PROJ_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${PROJ_BASENAME}.lib)
    elseif (OPENBSD)
      set(PROJ_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${PROJ_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.25.9.0.1)
    else (MSVC)
      set(PROJ_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${PROJ_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    endif (MSVC)
  endif (TARGET PROJ_BLD)

  if (TARGET SQLITE3_BLD)
    set(SQLITE3_TARGET SQLITE3_BLD)
    list(APPEND PROJ_DEPS SQLITE3_BLD libsqlite3_stage sqlite3_exe_stage)
    if (MSVC)
      set(SQLite3_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/libsqlite3.lib)
    elseif (OPENBSD)
      set(SQLite3_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/libsqlite3${CMAKE_SHARED_LIBRARY_SUFFIX}.3.32)
    else (MSVC)
      set(SQLite3_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/libsqlite3${CMAKE_SHARED_LIBRARY_SUFFIX})
    endif (MSVC)
    set(SQLite3_INCLUDE_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR})
    set(SQLite3_EXECNAME ${CMAKE_BINARY_ROOT}/${BIN_DIR}/sqlite3${CMAKE_EXECUTABLE_SUFFIX})
  endif (TARGET SQLITE3_BLD)

  if (MSVC)
    set(GDAL_BASENAME gdal)
    set(GDAL_STATICNAME gdal-static)
    set(GDAL_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(GDAL_SYMLINK_1 ${GDAL_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(GDAL_SYMLINK_2 ${GDAL_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.${GDAL_API_VERSION})
  elseif (APPLE)
    set(GDAL_BASENAME libgdal)
    set(GDAL_SUFFIX .${GDAL_API_VERSION}.${GDAL_MAJOR_VERSION}.${GDAL_MINOR_VERSION}.${GDAL_PATCH_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(GDAL_SYMLINK_1 ${GDAL_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(GDAL_SYMLINK_2 ${GDAL_BASENAME}.${GDAL_API_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX})
  elseif (OPENBSD)
    set(GDAL_BASENAME libgdal)
    set(GDAL_STATICNAME libgdal)
    set(GDAL_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${PROJ_API_VERSION}.0)
  else (MSVC)
    set(GDAL_BASENAME libgdal)
    set(GDAL_STATICNAME libgdal)
    set(GDAL_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${GDAL_API_VERSION}.${GDAL_MAJOR_VERSION}.${GDAL_MINOR_VERSION}.${GDAL_PATCH_VERSION})
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
    -DCMAKE_INSTALL_LIBDIR=${LIB_DIR}
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
    -DPROJ_LIBRARY=${PROJ_LIBRARY}
    -DPROJ_LIBRARY_RELEASE=${PROJ_LIBRARY}
    -DPROJ_LIBRARY_DEBUG=${PROJ_LIBRARY}
    -DGDAL_USE_SQLITE3=ON
    -DSQLite3_INCLUDE_DIR=${SQLite3_INCLUDE_DIR}
    -DSQLite3_LIBRARY=${SQLite3_LIBRARY}
    -DGDAL_BUILD_OPTIONAL_DRIVERS=OFF
    -DGDAL_ENABLE_DRIVER_RAW=ON # ACE2, BT, BYN, CPG, CTable2, DIPEx, DOQ1, DOQ2, EHDR, EIR, ENVI, FAST, GenBIN, GSC, GTX, MFF2, ISCE, KRO, MFF, LAN, LCP, LOSLAS, NDF, NTv2, PAUX, PNM, ROIPAC, RRASTER, SNODAS
    -DGDAL_ENABLE_DRIVER_PDS=ON # PDS, PDS4, ISIS2, ISIS3, VICAR
    -DGDAL_ENABLE_DRIVER_AAIGRID=ON # AAIGRID, GRASSASCIIGRID, ISG
    -DGDAL_ENABLE_DRIVER_GSG=ON # GSAG, GSBG, GS7BG
    -DGDAL_ENABLE_DRIVER_NITF=ON # NITF, RPFTOC, ECRGTOC
    -DGDAL_ENABLE_DRIVER_NORTHWOOD=ON # NWT_GRD, NWT_GRC
    -DGDAL_ENABLE_DRIVER_ADRG=ON # SRP, ADRG
    -DGDAL_ENABLE_DRIVER_AVC=ON # AVCBIN, AVCE00
    -DGDAL_ENABLE_DRIVER_AIGRID=ON
    -DGDAL_ENABLE_DRIVER_AIRSAR=ON
    -DGDAL_ENABLE_DRIVER_ARG=ON
    -DGDAL_ENABLE_DRIVER_BLX=ON
    -DGDAL_ENABLE_DRIVER_BMP=ON
    -DGDAL_ENABLE_DRIVER_BSB=ON
    -DGDAL_ENABLE_DRIVER_CALS=ON
    -DGDAL_ENABLE_DRIVER_CEOS=ON
    -DGDAL_ENABLE_DRIVER_COASP=ON
    -DGDAL_ENABLE_DRIVER_COSAR=ON
    -DGDAL_ENABLE_DRIVER_CTG=ON
    -DGDAL_ENABLE_DRIVER_DIMAP=ON
    -DGDAL_ENABLE_DRIVER_DTED=ON
    -DGDAL_ENABLE_DRIVER_ELAS=ON
    -DGDAL_ENABLE_DRIVER_ERS=ON
    -DGDAL_ENABLE_DRIVER_ENVISAT=ON
    -DGDAL_ENABLE_DRIVER_ESRIC=ON
    -DGDAL_ENABLE_DRIVER_FIT=ON
    -DGDAL_ENABLE_DRIVER_GFF=ON
    -DGDAL_ENABLE_DRIVER_GIF=ON
    -DGDAL_ENABLE_DRIVER_GRIB=ON
    -DGDAL_ENABLE_DRIVER_GXF=ON
    -DGDAL_ENABLE_DRIVER_HF2=ON
    -DGDAL_ENABLE_DRIVER_HFA=ON
    -DGDAL_ENABLE_DRIVER_IDRISI=ON
    -DGDAL_ENABLE_DRIVER_ILWIS=ON
    -DGDAL_ENABLE_DRIVER_IRIS=ON
    -DGDAL_ENABLE_DRIVER_JDEM=ON
    -DGDAL_ENABLE_DRIVER_JPEG=ON
    -DGDAL_ENABLE_DRIVER_KMLSUPEROVERLAY=ON
    -DGDAL_ENABLE_DRIVER_L1B=ON
    -DGDAL_ENABLE_DRIVER_LEVELLER=ON
    -DGDAL_ENABLE_DRIVER_MAP=ON
    -DGDAL_ENABLE_DRIVER_MRF=ON
    -DGDAL_ENABLE_DRIVER_MEM=ON
    -DGDAL_ENABLE_DRIVER_MSGN=ON
    -DGDAL_ENABLE_DRIVER_NGSGEOID=ON
    -DGDAL_ENABLE_DRIVER_OZI=ON
    -DGDAL_ENABLE_DRIVER_JAXAPALSAR=ON
    -DGDAL_ENABLE_DRIVER_PCIDSK=ON
    -DGDAL_ENABLE_DRIVER_PCRASTER=ON
    -DGDAL_ENABLE_DRIVER_PDF=ON
    -DGDAL_ENABLE_DRIVER_PNG=ON
    -DGDAL_ENABLE_DRIVER_PRF=ON
    -DGDAL_ENABLE_DRIVER_R=ON
    -DGDAL_ENABLE_DRIVER_RASTERLITE=ON
    -DGDAL_ENABLE_DRIVER_RIK=ON
    -DGDAL_ENABLE_DRIVER_RMF=ON
    -DGDAL_ENABLE_DRIVER_RS2=ON
    -DGDAL_ENABLE_DRIVER_SAFE=ON
    -DGDAL_ENABLE_DRIVER_SAR_CEOS=ON
    -DGDAL_ENABLE_DRIVER_SAGA=ON
    -DGDAL_ENABLE_DRIVER_SDTS=ON
    -DGDAL_ENABLE_DRIVER_SENTINEL2=ON
    -DGDAL_ENABLE_DRIVER_SGI=ON
    -DGDAL_ENABLE_DRIVER_SIGDEM=ON
    -DGDAL_ENABLE_DRIVER_SRTMHGT=ON
    -DGDAL_ENABLE_DRIVER_STACIT=ON
    -DGDAL_ENABLE_DRIVER_STACTA=ON
    -DGDAL_ENABLE_DRIVER_TERRAGEN=ON
    -DGDAL_ENABLE_DRIVER_TGA=ON
    -DGDAL_ENABLE_DRIVER_TIL=ON
    -DGDAL_ENABLE_DRIVER_TSX=ON
    -DGDAL_ENABLE_DRIVER_USGSDEM=ON
    -DGDAL_ENABLE_DRIVER_VRT=ON
    -DGDAL_ENABLE_DRIVER_XPM=ON
    -DGDAL_ENABLE_DRIVER_XYZ=ON
    -DGDAL_ENABLE_DRIVER_ZARR=ON
    -DGDAL_ENABLE_DRIVER_ZMAP=ON
    -DOGR_BUILD_OPTIONAL_DRIVERS=OFF
    -DOGR_ENABLE_DRIVER_AVC=ON
    -DOGR_ENABLE_DRIVER_CAD=ON
    -DOGR_ENABLE_DRIVER_CSV=ON
    -DOGR_ENABLE_DRIVER_DGN=ON
    -DOGR_ENABLE_DRIVER_DXF=ON
    -DOGR_ENABLE_DRIVER_EDIGEO=ON
    -DOGR_ENABLE_DRIVER_FLATGEOBUF=ON
    -DOGR_ENABLE_DRIVER_GEOCONCEPT=ON
    -DOGR_ENABLE_DRIVER_GPKG=ON
    -DOGR_ENABLE_DRIVER_GMT=ON
    -DOGR_ENABLE_DRIVER_IDRISI=ON
    -DOGR_ENABLE_DRIVER_MAPML=ON
    -DOGR_ENABLE_DRIVER_MITAB=ON
    -DOGR_ENABLE_DRIVER_NTF=ON
    -DOGR_ENABLE_DRIVER_PGDUMP=ON
    -DOGR_ENABLE_DRIVER_S57=ON
    -DOGR_ENABLE_DRIVER_SDTS=ON
    -DOGR_ENABLE_DRIVER_SELAFIN=ON
    -DOGR_ENABLE_DRIVER_SQLITE=ON
    -DOGR_ENABLE_DRIVER_SXF=ON
    -DOGR_ENABLE_DRIVER_TIGER=ON
    -DOGR_ENABLE_DRIVER_VDV=ON
    -DOGR_ENABLE_DRIVER_WASP=ON
    -DBUILD_PYTHON_BINDINGS=OFF
    -DBUILD_JAVA_BINDINGS=OFF
    -DBUILD_CSHARP_BINDINGS=OFF
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

mark_as_advanced(GDAL_INCLUDE_DIRS)
mark_as_advanced(GDAL_LIBRARIES)

include("${CMAKE_CURRENT_SOURCE_DIR}/gdal.dist")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
