set(gcore_srcs
	gcore/gdal_mdreader.cpp
	gcore/gdal_misc.cpp
	gcore/gdal_rat.cpp
	gcore/gdalabstractbandblockcache.cpp
	gcore/gdalallvalidmaskband.cpp
	gcore/gdalarraybandblockcache.cpp
	gcore/gdalclientserver.cpp
	gcore/gdalcolortable.cpp
	gcore/gdaldataset.cpp
	gcore/gdaldefaultasync.cpp
	gcore/gdaldefaultoverviews.cpp
	gcore/gdaldllmain.cpp
	gcore/gdaldriver.cpp
	gcore/gdaldrivermanager.cpp
	gcore/gdalexif.cpp
	gcore/gdalgeorefpamdataset.cpp
	gcore/gdalgmlcoverage.cpp
	gcore/gdalhashsetbandblockcache.cpp
	gcore/gdaljp2abstractdataset.cpp
	gcore/gdaljp2box.cpp
	gcore/gdaljp2metadata.cpp
	gcore/gdaljp2metadatagenerator.cpp
	gcore/gdaljp2structure.cpp
	gcore/gdalmajorobject.cpp
	gcore/gdalmultidomainmetadata.cpp
	gcore/gdalnodatamaskband.cpp
	gcore/gdalnodatavaluesmaskband.cpp
	gcore/gdalopeninfo.cpp
	gcore/gdaloverviewdataset.cpp
	gcore/gdalpamdataset.cpp
	gcore/gdalpamproxydb.cpp
	gcore/gdalpamrasterband.cpp
	gcore/gdalproxydataset.cpp
	gcore/gdalproxypool.cpp
	gcore/gdalrasterband.cpp
	gcore/gdalrasterblock.cpp
	gcore/gdalrescaledalphaband.cpp
	gcore/gdalvirtualmem.cpp
	gcore/overview.cpp
	gcore/rasterio.cpp
	)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/frmts/gtiff
	${CMAKE_CURRENT_SOURCE_DIR}/frmts/mem
	${CMAKE_CURRENT_SOURCE_DIR}/frmts/vrt
	${CMAKE_CURRENT_SOURCE_DIR}/ogr/ogrsf_frmts/generic
	${CMAKE_CURRENT_SOURCE_DIR}/gnm/gnm_frmts
	${CMAKE_CURRENT_SOURCE_DIR}/ogr/ogrsf_frmts/geojson/libjson
	${CMAKE_CURRENT_SOURCE_DIR}/ogr/ogrsf_frmts/geojson
)

#CPPFLAGS	:=	 -I../frmts/gtiff -I../frmts/mem -I../frmts/vrt -I../ogr -I../ogr/ogrsf_frmts/generic -I../gnm/ -I../gnm/gnm_frmts/ $(JSON_INCLUDE) -I../ogr/ogrsf_frmts/geojson $(CPPFLAGS) $(PAM_SETTING) $(XTRA_OPT)


