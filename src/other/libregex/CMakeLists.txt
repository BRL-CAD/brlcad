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
