set(gnm_srcs
	gnm/gnmgenericnetwork.cpp
	gnm/gnmgraph.cpp
	gnm/gnmlayer.cpp
	gnm/gnmnetwork.cpp
	gnm/gnmresultlayer.cpp
	gnm/gnmrule.cpp
	gnm/gnm_frmts/gnmregisterall.cpp
	gnm/gnm_frmts/file/gnmfiledriver.cpp
	gnm/gnm_frmts/file/gnmfilenetwork.cpp
	gnm/gnm_frmts/db/gnmdbdriver.cpp
	gnm/gnm_frmts/db/gnmdbnetwork.cpp
	)

include_directories(${GDAL_SOURCE_DIR}/gnm/gnm_frmts)
include_directories(${GDAL_SOURCE_DIR}/gnm/gnm_frmts/file)
include_directories(${GDAL_SOURCE_DIR}/gnm/gnm_frmts/db)
