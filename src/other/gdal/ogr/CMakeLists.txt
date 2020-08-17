add_subdirectory(libjson)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../frmts
  ${CMAKE_CURRENT_SOURCE_DIR}/libjson
  )

set(ogr_frmt_srcs
  ograeronavfaadriver.cpp ograeronavfaadatasource.cpp ograeronavfaalayer.cpp
  ograrcgendriver.cpp ograrcgendatasource.cpp ograrcgenlayer.cpp
  avc_bin.c
  avc_binwr.c
  avc_e00gen.c
  avc_e00parse.c
  avc_e00write.c
  avc_e00read.c
  avc_mbyte.c
  avc_misc.c
  avc_rawbin.c
  ogravcbindriver.cpp
  ogravcbindatasource.cpp
  ogravcbinlayer.cpp
  ogravclayer.cpp
  ogravcdatasource.cpp
  ogravce00layer.cpp
  ogravce00datasource.cpp
  ogravce00driver.cpp
  ogrbnadriver.cpp ogrbnadatasource.cpp ogrbnalayer.cpp ogrbnaparser.cpp
  ogrcsvdriver.cpp ogrcsvdatasource.cpp ogrcsvlayer.cpp
  ogrdgndriver.cpp
  ogrdgndatasource.cpp
  ogrdgnlayer.cpp
  dgnopen.cpp
  dgnread.cpp
  dgnfloat.cpp
  dgnhelp.cpp
  dgnwrite.cpp
  dgnstroke.cpp
  ogrdxfdriver.cpp
  ogrdxfdatasource.cpp
  ogrdxflayer.cpp
  ogrdxfreader.cpp
  ogrdxf_blockmap.cpp
  ogrdxf_dimension.cpp
  ogrdxfwriterds.cpp
  ogrdxfwriterlayer.cpp
  intronurbs.cpp
  ogrdxf_polyline_smooth.cpp
  ogrdxfblockslayer.cpp
  ogrdxfblockswriterlayer.cpp
  ogrdxf_hatch.cpp
  ogr_autocad_services.cpp
  ogredigeodriver.cpp ogredigeodatasource.cpp ogredigeolayer.cpp
  ogrsfdriverregistrar.cpp
  ogrlayer.cpp
  ogrdatasource.cpp
  ogrsfdriver.cpp
  ogrregisterall.cpp
  ogr_gensql.cpp
  ogr_attrind.cpp
  ogr_miattrind.cpp
  ogrlayerdecorator.cpp
  ogrwarpedlayer.cpp
  ogrunionlayer.cpp
  ogrlayerpool.cpp
  ogrmutexedlayer.cpp
  ogrmutexeddatasource.cpp
  ogremulatedtransaction.cpp
  ogreditablelayer.cpp
  geoconcept.c
  geoconcept_syscoord.c
  ogrgeoconceptdriver.cpp
  ogrgeoconceptdatasource.cpp
  ogrgeoconceptlayer.cpp
  ogrgeojsondriver.cpp
  ogrgeojsondatasource.cpp
  ogrgeojsonlayer.cpp
  ogrgeojsonwritelayer.cpp
  ogrgeojsonutils.cpp
  ogrgeojsonreader.cpp
  ogrgeojsonwriter.cpp
  ogresrijsonreader.cpp
  ogrtopojsonreader.cpp
  ogrgmtdriver.cpp ogrgmtdatasource.cpp ogrgmtlayer.cpp
  ogrgpsbabeldriver.cpp ogrgpsbabeldatasource.cpp ogrgpsbabelwritedatasource.cpp
  ogrgtmdriver.cpp
  ogrgtmdatasource.cpp
  ogrgtmlayer.cpp
  gtm.cpp
  gtmwaypointlayer.cpp
  gtmtracklayer.cpp

  ogrhtfdriver.cpp ogrhtfdatasource.cpp ogrhtflayer.cpp
  ogridrisidriver.cpp ogridrisidatasource.cpp ogridrisilayer.cpp
  ogrmemdriver.cpp ogrmemdatasource.cpp ogrmemlayer.cpp
  mitab_rawbinblock.cpp
  mitab_mapheaderblock.cpp
  mitab_mapindexblock.cpp
  mitab_mapobjectblock.cpp
  mitab_mapcoordblock.cpp
  mitab_feature.cpp
  mitab_feature_mif.cpp
  mitab_mapfile.cpp
  mitab_idfile.cpp
  mitab_datfile.cpp
  mitab_tabfile.cpp
  mitab_miffile.cpp
  mitab_utils.cpp
  mitab_imapinfofile.cpp
  mitab_middatafile.cpp
  mitab_bounds.cpp
  mitab_maptoolblock.cpp
  mitab_tooldef.cpp
  mitab_coordsys.cpp
  mitab_spatialref.cpp
  mitab_ogr_driver.cpp
  mitab_indfile.cpp
  mitab_tabview.cpp
  mitab_ogr_datasource.cpp
  mitab_geometry.cpp
  mitab_tabseamless.cpp
  ntffilereader.cpp
  ntfrecord.cpp
  ogrntfdatasource.cpp
  ogrntfdriver.cpp
  ogrntflayer.cpp
  ntf_estlayers.cpp
  ogrntffeatureclasslayer.cpp
  ntf_generic.cpp
  ntf_raster.cpp
  ntf_codelist.cpp
  ntfstroke.cpp
  ogropenairdriver.cpp
  ogropenairdatasource.cpp
  ogropenairlayer.cpp
  ogropenairlabellayer.cpp
  ogropenfilegdbdriver.cpp
  ogropenfilegdbdatasource.cpp
  ogropenfilegdblayer.cpp
  filegdbtable.cpp
  filegdbindex.cpp
  ogrpdsdriver.cpp ogrpdsdatasource.cpp ogrpdslayer.cpp
  ogrpgdumpdriver.cpp ogrpgdumpdatasource.cpp ogrpgdumplayer.cpp
  ogrrecdriver.cpp ogrrecdatasource.cpp ogrreclayer.cpp ll_recio.cpp
  ogrs57driver.cpp
  ogrs57datasource.cpp
  ogrs57layer.cpp
  s57reader.cpp
  s57writer.cpp
  ddfrecordindex.cpp
  s57classregistrar.cpp
  s57filecollector.cpp
  s57featuredefns.cpp
  ogrsegukooadriver.cpp ogrsegukooadatasource.cpp ogrsegukooalayer.cpp
  ogrsegydriver.cpp ogrsegydatasource.cpp ogrsegylayer.cpp
  ogrselafindriver.cpp ogrselafindatasource.cpp ogrselafinlayer.cpp io_selafin.cpp
  shape2ogr.cpp
  shpopen.c
  dbfopen.c
  shptree.c
  sbnsearch.c
  shp_vsi.c
  ogrshapedriver.cpp
  ogrshapedatasource.cpp
  ogrshapelayer.cpp
  ogrsuadriver.cpp ogrsuadatasource.cpp ogrsualayer.cpp
  ogrsxfdriver.cpp ogrsxfdatasource.cpp ogrsxflayer.cpp
  ogrtigerdriver.cpp
  ogrtigerdatasource.cpp
  ogrtigerlayer.cpp
  tigerfilebase.cpp
  tigercompletechain.cpp
  tigeraltname.cpp
  tigerfeatureids.cpp
  tigerzipcodes.cpp
  tigerlandmarks.cpp
  tigerarealandmarks.cpp
  tigerkeyfeatures.cpp
  tigerpolygon.cpp
  tigerpolygoncorrections.cpp
  tigerentitynames.cpp
  tigerpolygoneconomic.cpp
  tigeridhistory.cpp
  tigerpolychainlink.cpp
  tigerpip.cpp
  tigerspatialmetadata.cpp
  tigertlidrange.cpp
  tigerzerocellid.cpp
  tigeroverunder.cpp
  tigerzipplus4.cpp
  tigerpoint.cpp
  ogrvdvdatasource.cpp
  ogrvrtdatasource.cpp ogrvrtlayer.cpp ogrvrtdriver.cpp
  ogrwaspdriver.cpp ogrwaspdatasource.cpp ogrwasplayer.cpp
  ogrxplanedriver.cpp
  ogrxplanedatasource.cpp
  ogrxplanelayer.cpp
  ogr_xplane_geo_utils.cpp
  ogr_xplane_reader.cpp
  ogr_xplane_apt_reader.cpp
  ogr_xplane_nav_reader.cpp
  ogr_xplane_fix_reader.cpp
  ogr_xplane_awy_reader.cpp
  )

set(ogr_srcs
  ${ogr_frmt_srcs}
  ogrgeometryfactory.cpp
  ogrpoint.cpp
  ogrcurve.cpp
  ogrlinestring.cpp
  ogrlinearring.cpp
  ogrpolygon.cpp
  ogrtriangle.cpp
  ogrutils.cpp
  ogrgeometry.cpp
  ogrgeometrycollection.cpp
  ogrmultipolygon.cpp
  ogrsurface.cpp
  ogrpolyhedralsurface.cpp
  ogrtriangulatedsurface.cpp
  ogrmultipoint.cpp
  ogrmultilinestring.cpp
  ogrcircularstring.cpp
  ogrcompoundcurve.cpp
  ogrcurvepolygon.cpp
  ogrcurvecollection.cpp
  ogrmulticurve.cpp
  ogrmultisurface.cpp
  ogr_api.cpp
  ogrfeature.cpp
  ogrfeaturedefn.cpp
  ogrfeaturequery.cpp
  ogrfeaturestyle.cpp
  ogrfielddefn.cpp
  ogrspatialreference.cpp
  ogr_srsnode.cpp
  ogr_srs_proj4.cpp
  ogr_fromepsg.cpp
  ogrct.cpp
  ogr_opt.cpp
  ogr_srs_esri.cpp
  ogr_srs_pci.cpp
  ogr_srs_usgs.cpp
  ogr_srs_dict.cpp
  ogr_srs_panorama.cpp
  ogr_srs_ozi.cpp
  ogr_srs_erm.cpp
  swq.cpp
  swq_expr_node.cpp
  swq_parser.cpp
  swq_select.cpp
  swq_op_registrar.cpp
  swq_op_general.cpp
  ogr_srs_validate.cpp
  ogr_srs_xml.cpp
  ograssemblepolygon.cpp
  ogr2gmlgeometry.cpp
  gml2ogrgeometry.cpp
  ogr_expat.cpp
  ogrpgeogeometry.cpp
  ogrgeomediageometry.cpp
  ogr_geocoding.cpp
  osr_cs_wkt.c
  osr_cs_wkt_parser.c
  ogrgeomfielddefn.cpp
  ograpispy.cpp
  ogr_xerces.cpp
  )

foreach(fmt ${OGR_FORMATS})
  if(NOT "${fmt}" STREQUAL "generic")
    string(TOUPPER "${fmt}" UFMT)
    add_definitions(-D${UFMT}_ENABLED)
  endif(NOT "${fmt}" STREQUAL "generic")
endforeach(fmt ${OGR_FORMATS})
add_definitions(-DHAVE_MITAB)
add_definitions(-DMITAB_USE_OFTDATETIME)
add_definitions(-DOGR)
add_definitions(-DSAOffset=vsi_l_offset)
add_definitions(-DUSE_CPL)
add_definitions(-D_XOPEN_SOURCE=500)

add_library(gdal_ogr SHARED ${ogr_srcs} $<TARGET_OBJECTS:json_obj>)
set_target_properties(gdal_ogr PROPERTIES FOLDER "Third Party Libraries/GDAL")
install(TARGETS gdal_ogr
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
