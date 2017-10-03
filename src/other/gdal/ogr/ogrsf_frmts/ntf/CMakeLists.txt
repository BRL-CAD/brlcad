include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(ntf_srcs
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
	)

add_library(ogr_frmt_ntf OBJECT ${ntf_srcs})
