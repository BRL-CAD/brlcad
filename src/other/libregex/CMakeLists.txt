include_directories(${${CMAKE_PROJECT_NAME}_SOURCE_DIR}/include)

set(LIBREGEX_PUBLIC_HDRS
    regex.h
)

set(LIBREGEX_SOURCES
    regcomp.c
    regerror.c
    regexec.c
    regfree.c
)

add_library(regex SHARED ${LIBREGEX_SOURCES})
INSTALL(TARGETS regex DESTINATION lib)

add_library(regex-static STATIC ${LIBREGEX_SOURCES})
INSTALL(TARGETS regex-static ARCHIVE DESTINATION lib)
SET_TARGET_PROPERTIES(regex-static PROPERTIES OUTPUT_NAME "regex")

if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL)
   install(FILES ${LIBREGEX_PUBLIC_HDRS} DESTINATION include)
endif()
