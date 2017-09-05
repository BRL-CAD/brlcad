
include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/../gnm/gnm_frmts
	)

set(frmts_srcs frmts/gdalallregister.cpp)

foreach(frmt ${GDAL_FORMATS})
	add_definitions(-DFRMT_${frmt})
	set(GDAL_LIST "${GDAL_LIST} ${frmt}")
endforeach(frmt ${GDAL_FORMATS})

add_definitions(-DGDAL_FORMATS=${GDAL_LIST})

add_library(gdal_allreg OBJECT gdalallregister.cpp)


foreach(frmt ${GDAL_FORMATS})
	add_subdirectory(${frmt})
endforeach(frmt ${GDAL_FORMATS})
