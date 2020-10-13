
add_subdirectory(ogrsf_frmts)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/ogrsf_frmts
	${CMAKE_CURRENT_SOURCE_DIR}/ogrsf_frmts/mem
	${CMAKE_CURRENT_SOURCE_DIR}/ogrsf_frmts/geojson
	${CMAKE_CURRENT_SOURCE_DIR}/ogrsf_frmts/geojson/libjson
	${PROJ4_INCLUDE_DIRS}
	)

set(ogr_srcs
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

add_definitions(-DHAVE_MITAB)

add_library(gdal_ogr_obj OBJECT ${ogr_srcs})
set_target_properties(gdal_ogr_obj PROPERTIES FOLDER "Third Party Libraries/GDAL")
if(TARGET proj)
	add_dependencies(gdal_ogr_obj proj)
endif(TARGET proj)

