set(gnm_srcs
	gnmgenericnetwork.cpp
	gnmgraph.cpp
	gnmlayer.cpp
	gnmnetwork.cpp
	gnmresultlayer.cpp
	gnmrule.cpp
	gnm_frmts/gnmregisterall.cpp
	gnm_frmts/file/gnmfiledriver.cpp
	gnm_frmts/file/gnmfilenetwork.cpp
	gnm_frmts/db/gnmdbdriver.cpp
	gnm_frmts/db/gnmdbnetwork.cpp
	)

include_directories(${GDAL_SOURCE_DIR}/gnm/gnm_frmts)
include_directories(${GDAL_SOURCE_DIR}/gnm/gnm_frmts/file)
include_directories(${GDAL_SOURCE_DIR}/gnm/gnm_frmts/db)

add_library(gdal_gnm_obj OBJECT ${gnm_srcs})
set_target_properties(gdal_gnm_obj PROPERTIES FOLDER "Third Party Libraries/GDAL")
