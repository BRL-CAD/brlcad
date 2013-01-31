##
## Makefile for the view-dependent simplification package.
##
## Copyright 1999 The University of Virginia.  All Rights Reserved.  Disclaimer
## and copyright notice are given in full below and apply to this entire file.
##

## Set DBGOPT to -g, -O, etc.  Include -DVDS_DEBUGPRINT for verbose operation.
## Note that assertions are suppressed when VDS_DEBUGPRINT is not defined
DBGOPT = -g -DVDS_DEBUGPRINT
# DBGOPT = -Ofast=ip30
# DBGOPT = -O2

## Set these to the directories where binaries, libraries, and include files go
INST_BIN_DIR = $(DESTDIR)/usr/bin
INST_LIB_DIR = $(DESTDIR)/usr/lib
INST_INC_DIR = $(DESTDIR)/usr/include

## Edit the CC, CFLAGS, AR, and RANLIB compilation variables for your own
## system and preferences.
CC = cc
CFLAGS = ${DBGOPT}
AR = ar -cr
RANLIB = touch
DOC = doc++

##
## Set GLINC and GLLIB if necessary to specify where OpenGL include files 
## and libraries can be found (only needed for libstdvds.a)
##
GLINC = /usr/X11R6/include
GLLIB = /usr/X11R6/lib

##
## You shouldn't typically need to modify anything below this point.
##

COREOBJS 	= build.o dynamic.o render.o util.o file.o 
STDOBJS		= stdvds.o stdfold.o stdvis.o stdrender.o
ALLOBJS 	= $(COREOBJS) $(STDOBJS) 
HEADERS		= vds.h vdsprivate.h path.h vector.h stdvds.h
ALLSRCS		= $(ALLOBJS:.o=.c) ${HEADERS}

.c.o: 
	${CC} ${CFLAGS} -I${GLINC} -c $*.c

default: libvds.a libstdvds.a

all : libvds.a libstdvds.a

libvds.a : $(COREOBJS)
	$(AR) libvds.a $(COREOBJS)
	$(RANLIB) libvds.a

libstdvds.a : $(STDOBJS) 
	$(AR) libstdvds.a $(STDOBJS) 
	$(RANLIB) libstdvds.a

clean :
	rm -f $(ALLOBJS)
	rm -f libvds.a libstdvds.a 

clobber: clean

doc: $(ALLSRCS) doc.dxx basics.dxx
	# Run the doc++ package to extract documentation from the source:
	$(DOC) -H -B /home/luebke/public_html/copyright.html -d html doc.dxx

$(ALLOBJS): $(HEADERS)

## Rule for installing binaries and header files.  Replace the directories
## (above) with whatever is appropriate for your site
install: all 
	@ echo "Installing VDSlib compiled with $(DBGOPT)"
	cp libvds.a libstdvds.a $(INST_LIB_DIR)
	cp vds.h stdvds.h $(INST_INC_DIR)
#	@ echo "Compiling clean version of polyview"
#	cd polyview; make clobber polyview
	@ echo "Installing polyview binary to $(INST_BIN_DIR)"
	cp polyview/polyview $(INST_BIN_DIR)
	@ echo "Done. Don't forget to recompile any apps that depend on VDSlib"

## Rules for publishing the vdslib to a web page.  Users of the 
## library shouldn't need to worry about this.
VERSION = 0.9
PUBDIR = /var/ftp/pub/vdslib
DOCDIR = /home/luebke/public_html/vdslib/vdsdoc
publish: doc clean
	@ echo "Publishing up-to-date documentation on web site"
	cp -r html/* $(DOCDIR)
	@ echo "Cleaning polyview directory"
	cd polyview; make clobber
	rm -rf vdslib$(VERSION)
	mkdir vdslib$(VERSION)
	cp $(ALLSRCS) Makefile vdslib$(VERSION)
	cp -r polyview vdslib$(VERSION)
	tar cf vdslib.tar vdslib$(VERSION)
	rm -rf vdslib$(VERSION)
	gzip vdslib.tar
	mv vdslib.tar.gz $(PUBDIR)

##########################################################################
# 
#   Copyright 1999 The University of Virginia.
#   All Rights Reserved.
#
#   Permission to use, copy, modify and distribute this software and its
#   documentation without fee, and without a written agreement, is
#   hereby granted, provided that the above copyright notice and the
#   complete text of this comment appear in all copies, and provided that
#   the University of Virginia and the original authors are credited in
#   any publications arising from the use of this software.
# 
#   IN NO EVENT SHALL THE UNIVERSITY OF VIRGINIA
#   OR THE AUTHOR OF THIS SOFTWARE BE LIABLE TO ANY PARTY FOR DIRECT,
#   INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
#   LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
#   DOCUMENTATION, EVEN IF THE UNIVERSITY OF VIRGINIA AND/OR THE
#   AUTHOR OF THIS SOFTWARE HAVE BEEN ADVISED OF THE POSSIBILITY OF 
#   SUCH DAMAGES.
# 
#   The author of the vdslib software library may be contacted at:
# 
#   US Mail:             Dr. David Patrick Luebke
#                        Department of Computer Science
#                        Thornton Hall, University of Virginia
# 		       Charlottesville, VA 22903
# 
#   Phone:               (804)924-1021
# 
#   EMail:               luebke@cs.virginia.edu
# 
##########################################################################
