include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(dxf_srcs
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
	)

add_library(ogr_frmt_dxf OBJECT ${dxf_srcs})
