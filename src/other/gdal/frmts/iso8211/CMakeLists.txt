include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(iso8211_srcs
	ddfmodule.cpp
	ddfutils.cpp
	ddffielddefn.cpp
	ddfrecord.cpp
	ddffield.cpp
	ddfsubfielddefn.cpp
	)

add_library(frmt_iso8211 OBJECT ${iso8211_srcs})
