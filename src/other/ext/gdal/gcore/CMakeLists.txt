add_subdirectory(mdreader)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/../frmts/gtiff
	${CMAKE_CURRENT_SOURCE_DIR}/../frmts/mem
	${CMAKE_CURRENT_SOURCE_DIR}/../frmts/vrt
	${CMAKE_CURRENT_SOURCE_DIR}/../ogr/ogrsf_frmts/generic
	${CMAKE_CURRENT_SOURCE_DIR}/../gnm/gnm_frmts
	${CMAKE_CURRENT_SOURCE_DIR}/../ogr/ogrsf_frmts/geojson/libjson
	${CMAKE_CURRENT_SOURCE_DIR}/../ogr/ogrsf_frmts/geojson
)


set(gcore_srcs
	gdalopeninfo.cpp
	gdaldrivermanager.cpp
	gdaldriver.cpp
	gdaldataset.cpp
	gdalrasterband.cpp
	gdal_misc.cpp
	rasterio.cpp
	gdalrasterblock.cpp
	gdalcolortable.cpp
	gdalmajorobject.cpp
	overview.cpp
	gdaldefaultoverviews.cpp
	gdalpamdataset.cpp
	gdalpamrasterband.cpp
	gdaljp2metadata.cpp
	gdaljp2box.cpp
	gdalmultidomainmetadata.cpp
	gdal_rat.cpp
	gdalgmlcoverage.cpp
	gdalpamproxydb.cpp
	gdalallvalidmaskband.cpp
	gdalnodatamaskband.cpp
	gdalproxydataset.cpp
	gdalproxypool.cpp
	gdaldefaultasync.cpp
	gdalnodatavaluesmaskband.cpp
	gdaldllmain.cpp
	gdalexif.cpp
	gdalclientserver.cpp
	gdalgeorefpamdataset.cpp
	gdaljp2abstractdataset.cpp
	gdalvirtualmem.cpp
	gdaloverviewdataset.cpp
	gdalrescaledalphaband.cpp
	gdaljp2structure.cpp
	gdal_mdreader.cpp
	gdaljp2metadatagenerator.cpp
	gdalabstractbandblockcache.cpp
	gdalarraybandblockcache.cpp
	gdalhashsetbandblockcache.cpp
	rasterio_ssse3.cpp
	)

if(C_SSSE3_FOUND)
  set_property(SOURCE rasterio_ssse3.cpp APPEND PROPERTY COMPILE_FLAGS "${C_SSSE3_FLAGS}")
endif(C_SSSE3_FOUND)

add_library(gdal_gcore_obj OBJECT ${gcore_srcs})
set_target_properties(gdal_gcore_obj PROPERTIES FOLDER "Third Party Libraries/GDAL")
