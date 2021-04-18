include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(tiger_srcs
	ogrtigerdriver.cpp
	ogrtigerdatasource.cpp
	ogrtigerlayer.cpp
	tigerfilebase.cpp
	tigercompletechain.cpp
	tigeraltname.cpp
	tigerfeatureids.cpp
	tigerzipcodes.cpp
	tigerlandmarks.cpp
	tigerarealandmarks.cpp
	tigerkeyfeatures.cpp
	tigerpolygon.cpp
	tigerpolygoncorrections.cpp
	tigerentitynames.cpp
	tigerpolygoneconomic.cpp
	tigeridhistory.cpp
	tigerpolychainlink.cpp
	tigerpip.cpp
	tigerspatialmetadata.cpp
	tigertlidrange.cpp
	tigerzerocellid.cpp
	tigeroverunder.cpp
	tigerzipplus4.cpp
	tigerpoint.cpp
	)

add_library(ogr_frmt_tiger OBJECT ${tiger_srcs})
