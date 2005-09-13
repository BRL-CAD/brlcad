# zlibtcl.decls -- -*- tcl -*-
#
# This file contains the declarations for all supported public functions
# that are exported by the ZLIBTCL library via the stubs table. This file
# is used to generate the zlibtclDecls.h/zlibtclStubsLib.c/zlibtclStubsInit.c
# files.
#	

# Declare each of the functions in the public BLT interface.  Note that
# the an index should never be reused for a different function in order
# to preserve backwards compatibility.

library zlibtcl

# Define the ZLIBTCL interface:

interface zlibtcl
#hooks {}

#########################################################################
###  Misc. interfaces

declare 0 generic {
    CONST char* zlibVersion (void)
}

declare 1 generic {
    CONST char* zError (int err)
}

declare 2 generic {
    uLong crc32 (uLong crc, const Bytef *buf, uInt len)
}

declare 3 generic {
    uLong adler32 (uLong adler, const Bytef *buf, uInt len)
}

#########################################################################
###  Deflate = Compression

declare 10 generic {
    int deflateInit_ (z_streamp stream, int level, const char* version, int stream_size)
}
declare 11 generic {
    int deflateInit2_ (z_streamp stream, int level, 
	int method, int windowBits, int memLevel, int strategy,
	const char* version, int stream_size)
}
declare 12 generic {
    int deflate (z_streamp stream, int flush)
}
declare 13 generic {
    int deflateEnd (z_streamp stream)
}
declare 14 generic {
    int deflateSetDictionary (z_streamp stream, CONST Bytef* dict, uInt dictLength)
}
declare 15 generic {
    int deflateCopy (z_streamp dst, z_streamp src)
}
declare 16 generic {
    int deflateReset (z_streamp stream)
}
declare 17 generic {
    int deflateParams (z_streamp stream, int level, int strategy)
}

#########################################################################

declare 18 generic {
    int compress (Bytef *dest, uLongf *destLen, CONST Bytef *source, uLong sourceLen)
}
declare 19 generic {
    int compress2 (Bytef *dest, uLongf *destLen, CONST Bytef *source, uLong sourceLen, int level)
}

#########################################################################
###  Inflate = Decompression

declare 20 generic {
    int inflateInit_ (z_streamp stream, const char* version, int stream_size)
}
declare 21 generic {
    int inflateInit2_ (z_streamp stream, int windowBits, const char* version,
			int stream_size)
}
declare 22 generic {
    int inflate (z_streamp stream, int flush)
}
declare 23 generic {
    int inflateEnd (z_streamp stream)
}
declare 24 generic {
    int inflateSetDictionary (z_streamp stream, CONST Bytef* dict, uInt dictLength)
}
declare 25 generic {
    int inflateSync (z_streamp stream)
}
declare 26 generic {
    int inflateReset (z_streamp stream)
}

#########################################################################

declare 27 generic {
    int uncompress (Bytef *dest, uLongf *destLen, CONST Bytef *source, uLong sourceLen)
}

#########################################################################
## gz'ip layer

declare 30 generic {
    gzFile gzopen  (const char *path, const char *mode)
}
declare 31 generic {
    gzFile gzdopen  (int fd, const char *mode)
}
declare 32 generic {
    int gzsetparams (gzFile file, int level, int strategy)
}
declare 33 generic {
    int gzread  (gzFile file, voidp buf, unsigned len)
}
declare 34 generic {
    int gzwrite (gzFile file, const voidpc buf, unsigned len)
}
# gzprintf - signature cannot be handled by current stubs mechanism
if {0} {
    declare 35 generic {
	int gzprintf (gzFile file, const char *format, ...)
    }
}
declare 36 generic {
    int gzputs (gzFile file, const char *s)
}
declare 37 generic {
    char* gzgets (gzFile file, char *buf, int len)
}
declare 38 generic {
    int gzputc (gzFile file, int c)
}
declare 39 generic {
    int gzgetc (gzFile file)
}
declare 40 generic {
    int gzflush (gzFile file, int flush)
}
declare 41 generic {
    z_off_t gzseek (gzFile file, z_off_t offset, int whence)
}
declare 42 generic {
    int gzrewind (gzFile file)
}
declare 43 generic {
    z_off_t gztell (gzFile file)
}
declare 44 generic {
    int gzeof (gzFile file)
}
declare 45 generic {
    int gzclose (gzFile file)
}
declare 46 generic {
    const char* gzerror (gzFile file, int *errnum)
}

#########################################################################
