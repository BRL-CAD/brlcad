set(LIBZ_SOURCES
    adler32.c
    compress.c
    crc32.c
    deflate.c
    gzio.c
    infback.c
    inffast.c
    inflate.c
    inftrees.c
    trees.c
    uncompr.c
    zutil.c
)

set(LIBZ_HEADERS
    zconf.h
    zlib.h
)

add_library(libz ${LIBZ_SOURCES})

install(FILES ${LIBZ_HEADERS} DESTINATION include)
