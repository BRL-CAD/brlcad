include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(hfa_srcs
	hfaopen.cpp
	hfaentry.cpp
	hfadictionary.cpp
	hfafield.cpp
	hfatype.cpp
	hfaband.cpp
	hfacompress.cpp
	hfadataset.cpp
	hfa_overviews.cpp
	)

add_library(frmt_hfa OBJECT ${hfa_srcs})
