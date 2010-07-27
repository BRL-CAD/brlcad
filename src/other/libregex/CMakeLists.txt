set(LIBREGEX_PUBLIC_HDRS
    regex.h
)

set(LIBREGEX_SOURCES
    regcomp.c
    regerror.c
    regexec.c
    regfree.c
)

include_directories(
    ../../../include
)

add_library(libregex ${LIBREGEX_SOURCES})

if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL)
   install(FILES ${LIBREGEX_PUBLIC_HDRS} DESTINATION include)
endif()
