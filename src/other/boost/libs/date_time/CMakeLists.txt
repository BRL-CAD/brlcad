include_directories(
	../..
	)

SET(date_time_SRCS
	src/gregorian/greg_month.cpp
	src/gregorian/greg_weekday.cpp
	src/gregorian/date_generators.cpp
	)

add_definitions(-DDATE_TIME_INLINE)

add_library(boost_date_time SHARED ${date_time_SRCS})
set_target_properties(boost_date_time PROPERTIES LINK_FLAGS	"-DBOOST_ALL_DYN_LINK=1 -DBOOST_DATE_TIME_DYN_LINK")

add_library(boost_date_time_static STATIC ${date_time_SRCS})
set_target_properties(boost_date_time_static PROPERTIES LINK_FLAGS "-DBOOST_DATE_TIME_STATIC_LINK")
