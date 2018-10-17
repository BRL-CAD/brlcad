
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

if(C_AVX_FOUND)
  set_property(SOURCE gdalgridavx.cpp APPEND PROPERTY COMPILE_FLAGS "${C_AVX_FLAGS}")
endif(C_AVX_FOUND)

if(C_SSE_FOUND)
  set_property(SOURCE gdalgridsse.cpp APPEND PROPERTY COMPILE_FLAGS "${C_SSE_FLAGS}")
endif(C_SSE_FOUND)

add_library(gdal_alg_obj OBJECT ${alg_srcs})
set_target_properties(gdal_alg_obj PROPERTIES FOLDER "Third Party Libraries/GDAL")
