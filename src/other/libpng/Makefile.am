
lib_LTLIBRARIES = libpng.la

# NOTE: either PNG_NO_MMX_CODE or PNG_NO_ASSEMBLER_CODE are required
# for successful compilation on AMD64 Linux with gcc due to linking of
# 32-bit assembly with 64-bit object files.  Compiling with -m32 also
# avoids the problem.
libpng_la_CFLAGS = ${LIBZ_CPPFLAGS} -DPNG_NO_MMX_CODE
libpng_la_LDFLAGS = -version-info 3:5:2
libpng_la_LIBADD = ${LIBZ}
libpng_la_SOURCES = \
	png.c \
	pngerror.c \
	pngget.c \
	pngmem.c \
	pngpread.c \
	pngread.c \
	pngrio.c \
	pngrtran.c \
	pngrutil.c \
	pngset.c \
	pngtest.c \
	pngtrans.c \
	pngwio.c \
	pngwrite.c \
	pngwtran.c \
	pngwutil.c

noinst_PROGRAMS = pngtest

pngtest_SOURCES = pngtest.c
pngtest_CFLAGS = ${LIBZ_CPPFLAGS}
pngtest_LDADD = \
	${lib_LTLIBRARIES} \
	${LIBZ} \
	${LIBM}

include_HEADERS = \
	png.h \
	pngconf.h \
	pngpriv.h

dist_man_MANS = \
	libpng.3 \
	libpngpf.3 \
	png.5

EXTRA_DIST = \
	ANNOUNCE \
	CHANGES \
	CMakeLists.txt \
	LICENSE \
	README \
	TODO \
	contrib/gregbook/COPYING \
	contrib/gregbook/LICENSE \
	contrib/gregbook/Makefile.mingw32 \
	contrib/gregbook/Makefile.sgi \
	contrib/gregbook/Makefile.unx \
	contrib/gregbook/Makefile.w32 \
	contrib/gregbook/README \
	contrib/gregbook/makevms.com \
	contrib/gregbook/readpng.c \
	contrib/gregbook/readpng.h \
	contrib/gregbook/readpng2.c \
	contrib/gregbook/readpng2.h \
	contrib/gregbook/readppm.c \
	contrib/gregbook/rpng-win.c \
	contrib/gregbook/rpng-x.c \
	contrib/gregbook/rpng2-win.c \
	contrib/gregbook/rpng2-x.c \
	contrib/gregbook/wpng.c \
	contrib/gregbook/writepng.c \
	contrib/gregbook/writepng.h \
	contrib/pngminim/decoder/README \
	contrib/pngminim/decoder/gather.sh \
	contrib/pngminim/decoder/makefile \
	contrib/pngminim/decoder/pngusr.h \
	contrib/pngminim/encoder/README \
	contrib/pngminim/encoder/dummy_inflate.c \
	contrib/pngminim/encoder/gather.sh \
	contrib/pngminim/encoder/makefile \
	contrib/pngminim/encoder/pngusr.h \
	contrib/pngminim/preader/README \
	contrib/pngminim/preader/gather.sh \
	contrib/pngminim/preader/makefile \
	contrib/pngminim/preader/pngusr.h \
	contrib/pngminus/README \
	contrib/pngminus/makefile.std \
	contrib/pngminus/makefile.tc3 \
	contrib/pngminus/makevms.com \
	contrib/pngminus/png2pnm.bat \
	contrib/pngminus/png2pnm.c \
	contrib/pngminus/png2pnm.sh \
	contrib/pngminus/pngminus.bat \
	contrib/pngminus/pngminus.sh \
	contrib/pngminus/pnm2png.bat \
	contrib/pngminus/pnm2png.c \
	contrib/pngminus/pnm2png.sh \
	contrib/visupng/PngFile.c \
	contrib/visupng/PngFile.h \
	contrib/visupng/README.txt \
	contrib/visupng/VisualPng.c \
	contrib/visupng/VisualPng.dsp \
	libpng-1.4.1.txt \
	libpng-config.in \
	libpng.pc.in \
	projects/visualc6/README.txt \
	projects/visualc6/libpng.dsp \
	projects/visualc6/pngtest.dsp \
	projects/visualc71/PRJ0041.mak \
	projects/visualc71/README.txt \
	projects/visualc71/README_zlib.txt \
	projects/visualc71/libpng.sln \
	projects/visualc71/libpng.vcproj \
	projects/visualc71/pngtest.vcproj \
	projects/visualc71/zlib.vcproj \
	projects/xcode/Info.plist \
	projects/xcode/README.txt \
	projects/xcode/libpng.xcodeproj/.gitignore \
	projects/xcode/libpng.xcodeproj/project.pbxproj \
	scripts/README.txt \
	scripts/descrip.mms \
	scripts/libpng-config-head.in \
	scripts/libpng.pc.in \
	scripts/makefile.32sunu \
	scripts/makefile.64sunu \
	scripts/makefile.acorn \
	scripts/makefile.aix \
	scripts/makefile.amiga \
	scripts/makefile.atari \
	scripts/makefile.bc32 \
	scripts/makefile.beos \
	scripts/makefile.bor \
	scripts/makefile.cegcc \
	scripts/makefile.cygwin \
	scripts/makefile.darwin \
	scripts/makefile.dec \
	scripts/makefile.dj2 \
	scripts/makefile.elf \
	scripts/makefile.freebsd \
	scripts/makefile.gcc \
	scripts/makefile.hp64 \
	scripts/makefile.hpgcc \
	scripts/makefile.hpux \
	scripts/makefile.ibmc \
	scripts/makefile.intel \
	scripts/makefile.knr \
	scripts/makefile.linux \
	scripts/makefile.mingw \
	scripts/makefile.mips \
	scripts/makefile.msc \
	scripts/makefile.ne12bsd \
	scripts/makefile.netbsd \
	scripts/makefile.openbsd \
	scripts/makefile.os2 \
	scripts/makefile.sco \
	scripts/makefile.sggcc \
	scripts/makefile.sgi \
	scripts/makefile.so9 \
	scripts/makefile.solaris \
	scripts/makefile.solaris-x86 \
	scripts/makefile.std \
	scripts/makefile.sunos \
	scripts/makefile.tc3 \
	scripts/makefile.vcwin32 \
	scripts/makefile.watcom \
	scripts/makevms.com \
	scripts/png32ce.def \
	scripts/pngos2.def \
	scripts/pngwin.def \
	scripts/pngwin.rc \
	scripts/smakefile.ppc

.PHONY : test
test:	pngtest
	./pngtest ${top_srcdir}/src/other/libpng/pngtest.png

if BUILD_ZLIB
DEPADD = src/other/libz
endif

DEPENDS = ${DEPADD}

include $(top_srcdir)/misc/Makefile.defs

