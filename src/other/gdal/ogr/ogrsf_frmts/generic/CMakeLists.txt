include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/..
	)

foreach(fmt ${OGR_FORMATS})
	if(NOT "${fmt}" STREQUAL "generic")
		string(TOUPPER "${fmt}" UFMT)
		add_definitions(-D${UFMT}_ENABLED)
	endif(NOT "${fmt}" STREQUAL "generic")
endforeach(fmt ${OGR_FORMATS})

set(generic_srcs
	ogrsfdriverregistrar.cpp
	ogrlayer.cpp
	ogrdatasource.cpp
	ogrsfdriver.cpp
	ogrregisterall.cpp
	ogr_gensql.cpp
	ogr_attrind.cpp
	ogr_miattrind.cpp
	ogrlayerdecorator.cpp
	ogrwarpedlayer.cpp
	ogrunionlayer.cpp
	ogrlayerpool.cpp
	ogrmutexedlayer.cpp
	ogrmutexeddatasource.cpp
	ogremulatedtransaction.cpp
	ogreditablelayer.cpp
	)

add_library(ogr_frmt_generic OBJECT ${generic_srcs})
