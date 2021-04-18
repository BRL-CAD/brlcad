include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_definitions(-DUSE_CPL)

set(geoconcept_srcs
	geoconcept.c
	geoconcept_syscoord.c
	ogrgeoconceptdriver.cpp
	ogrgeoconceptdatasource.cpp
	ogrgeoconceptlayer.cpp
	)

add_library(ogr_frmt_geoconcept OBJECT ${geoconcept_srcs})
