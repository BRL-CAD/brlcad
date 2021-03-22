include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(dgn_srcs
	ogrdgndriver.cpp
	ogrdgndatasource.cpp
	ogrdgnlayer.cpp
	dgnopen.cpp
	dgnread.cpp
	dgnfloat.cpp
	dgnhelp.cpp
	dgnwrite.cpp
	dgnstroke.cpp
	)

add_library(ogr_frmt_dgn OBJECT ${dgn_srcs})
