
include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/../frmts/vrt
	)

add_definitions(-DINTERNAL_QHULL)

set(alg_srcs
	gdalmediancut.cpp
	gdaldither.cpp
	gdal_crs.c
	gdaltransformer.cpp
	gdalsimplewarp.cpp
	gdalwarper.cpp
	gdalwarpkernel.cpp
	gdalwarpoperation.cpp
	gdalchecksum.cpp
	gdal_rpc.cpp
	gdal_tps.cpp
	thinplatespline.cpp
	llrasterize.cpp
	gdalrasterize.cpp
	gdalgeoloc.cpp
	gdalgrid.cpp
	gdalcutline.cpp
	gdalproximity.cpp
	rasterfill.cpp
	gdalrasterpolygonenumerator.cpp
	gdalsievefilter.cpp
	gdalwarpkernel_opencl.cpp
	polygonize.cpp
	contour.cpp
	gdaltransformgeolocs.cpp
	gdal_octave.cpp
	gdal_simplesurf.cpp
	gdalmatching.cpp
	delaunay.c
	gdalpansharpen.cpp
	gdalapplyverticalshiftgrid.cpp
	gdalgridavx.cpp
	gdalgridsse.cpp
	)

set_property(SOURCE gdalgridavx.cpp APPEND PROPERTY COMPILE_FLAGS "-mavx")

add_library(gdal_alg_obj OBJECT ${alg_srcs})
set_target_properties(gdal_alg_obj PROPERTIES FOLDER "Third Party Libraries/gdal")
