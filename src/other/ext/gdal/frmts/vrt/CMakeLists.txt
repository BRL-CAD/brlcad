include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../raw
	)

set(vrt_srcs
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
	)

add_library(frmt_vrt OBJECT ${vrt_srcs})
