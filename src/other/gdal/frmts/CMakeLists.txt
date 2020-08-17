add_subdirectory(libtiff)
add_subdirectory(libgeotiff)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../gnm/gnm_frmts
  ${CMAKE_CURRENT_SOURCE_DIR}/../ogr/ogrsf_frmts/avc
  ${CMAKE_CURRENT_SOURCE_DIR}/../ogr/ogrsf_frmts/geojson
  ${CMAKE_CURRENT_SOURCE_DIR}/../ogr/ogrsf_frmts/geojson/libjson
  ${CMAKE_CURRENT_SOURCE_DIR}/../ogr/ogrsf_frmts/shape
  ${CMAKE_CURRENT_SOURCE_DIR}/gtiff
  ${CMAKE_CURRENT_SOURCE_DIR}/gtiff/libtiff
  ${CMAKE_CURRENT_SOURCE_DIR}/iso8211
  ${CMAKE_CURRENT_SOURCE_DIR}/libgeotiff
  ${CMAKE_CURRENT_SOURCE_DIR}/libtiff
  ${CMAKE_CURRENT_SOURCE_DIR}/mem
  ${CMAKE_CURRENT_SOURCE_DIR}/raw
  ${CMAKE_CURRENT_SOURCE_DIR}/vrt
  )

add_definitions(-DGDALDRIVER)
add_definitions(-DINTERNAL_LIBGEOTIFF)
add_definitions(-DINTERNAL_LIBTIFF)
add_definitions(-DBIGTIFF_SUPPORT)

foreach(frmt ${GDAL_FORMATS})
  add_definitions(-DFRMT_${frmt})
  set(GDAL_LIST "${GDAL_LIST} ${frmt}")
endforeach(frmt ${GDAL_FORMATS})
add_definitions(-DGDAL_FORMATS=${GDAL_LIST})

set(gdal_frmt_srcs
  gdalallregister.cpp
  aaigriddataset.cpp
  adrgdataset.cpp srpdataset.cpp
  aigdataset.cpp gridlib.c aigopen.c aigccitt.c
  airsardataset.cpp
  argdataset.cpp
  blxdataset.cpp blx.c
  bmpdataset.cpp
  calsdataset.cpp
  sar_ceosdataset.cpp ceosrecipe.c ceossar.c ceos.c link.c
  ceosopen.c ceosdataset.cpp
  coasp_dataset.cpp
  cosar_dataset.cpp
  ctgdataset.cpp
  deriveddataset.cpp derivedlist.c
  dimapdataset.cpp
  dted_api.c dteddataset.cpp dted_create.c dted_ptstream.c
  e00griddataset.cpp
  elasdataset.cpp
  EnvisatFile.c records.c adsrange.cpp unwrapgcps.cpp envisatdataset.cpp
  ersdataset.cpp ershdrnode.cpp
  fitdataset.cpp fit.cpp
  gff_dataset.cpp
  gsagdataset.cpp gsbgdataset.cpp gs7bgdataset.cpp
  geotiff.cpp
  gt_wkt_srs.cpp
  gt_citation.cpp
  gt_overview.cpp
  tif_float.c
  tifvsi.cpp
  gt_jpeg_copy.cpp
  gxfdataset.cpp gxfopen.c gxf_proj4.c gxf_ogcwkt.c
  hf2dataset.cpp
  hfaopen.cpp
  hfaentry.cpp
  hfadictionary.cpp
  hfafield.cpp
  hfatype.cpp
  hfaband.cpp
  hfacompress.cpp
  hfadataset.cpp
  hfa_overviews.cpp
  IdrisiDataset.cpp
  ilwisdataset.cpp ilwiscoordinatesystem.cpp
  IntergraphDataset.cpp IntergraphBand.cpp IngrTypes.cpp JpegHelper.cpp
  irisdataset.cpp
  ddfmodule.cpp
  ddfutils.cpp
  ddffielddefn.cpp
  ddfrecord.cpp
  ddffield.cpp
  ddfsubfielddefn.cpp
  jaxapalsardataset.cpp
  jdemdataset.cpp
  kmlsuperoverlaydataset.cpp
  l1bdataset.cpp
  levellerdataset.cpp
  mapdataset.cpp
  memdataset.cpp
  ngsgeoiddataset.cpp
  nitfdataset.cpp
  rpftocdataset.cpp
  nitfwritejpeg.cpp
  nitfwritejpeg_12.cpp
  nitfrasterband.cpp
  ecrgtocdataset.cpp
  nitffile.c
  nitfimage.c
  mgrs.c
  nitfaridpcm.cpp
  nitfbilevel.cpp
  rpftocfile.cpp
  nitfdes.c
  nitf_gcprpc.cpp
  grddataset.cpp grcdataset.cpp northwood.cpp
  ozidataset.cpp
  pdsdataset.cpp
  isis2dataset.cpp
  isis3dataset.cpp
  vicardataset.cpp
  nasakeywordhandler.cpp
  vicarkeywordhandler.cpp
  pngdataset.cpp
  phprfdataset.cpp
  rawdataset.cpp
  ehdrdataset.cpp
  pauxdataset.cpp
  doq1dataset.cpp
  doq2dataset.cpp
  mffdataset.cpp
  hkvdataset.cpp
  pnmdataset.cpp
  fujibasdataset.cpp
  envidataset.cpp
  gscdataset.cpp
  fastdataset.cpp
  atlsci_spheroid.cpp
  btdataset.cpp
  landataset.cpp
  cpgdataset.cpp
  idadataset.cpp
  ndfdataset.cpp
  dipxdataset.cpp
  genbindataset.cpp
  lcpdataset.cpp
  eirdataset.cpp
  gtxdataset.cpp
  loslasdataset.cpp
  ntv2dataset.cpp
  ace2dataset.cpp
  snodasdataset.cpp
  ctable2dataset.cpp
  krodataset.cpp
  roipacdataset.cpp
  iscedataset.cpp
  rrasterdataset.cpp
  rdataset.cpp rcreatecopy.cpp
  rikdataset.cpp
  rmfdataset.cpp rmflzw.cpp rmfdem.cpp
  rs2dataset.cpp
  safedataset.cpp
  sagadataset.cpp
  sentinel2dataset.cpp
  sgidataset.cpp
  srtmhgtdataset.cpp
  terragendataset.cpp
  tildataset.cpp
  tsxdataset.cpp
  usgsdemdataset.cpp usgsdem_create.cpp
  vrtdataset.cpp
  vrtrasterband.cpp
  vrtdriver.cpp
  vrtsources.cpp
  vrtfilters.cpp
  vrtsourcedrasterband.cpp
  vrtrawrasterband.cpp
  vrtwarped.cpp
  vrtderivedrasterband.cpp
  vrtpansharpened.cpp
  pixelfunctions.cpp
  xpmdataset.cpp
  xyzdataset.cpp
  zmapdataset.cpp
  )

add_library(gdal_frmts_obj OBJECT ${gdal_frmt_srcs})
set_target_properties(gdal_frmts_obj PROPERTIES FOLDER "Third Party Libraries/GDAL")

if (TARGET png)
  add_dependencies(gdal_frmts_obj png)
endif (TARGET png)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

