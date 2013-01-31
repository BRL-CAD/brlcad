# Set to whatever compiler you want
CC = cc

# Set these to the directories containing VDSlib, XForms and (if needed) Mesa:
#
# 	Compiler flags
#
VDSDIR = ..
VDSINC = -I$(VDSDIR)
GFXINC = -I/home/gfx/include
CFLAGS = $(OPTFLAG) $(VDSINC) $(GFXINC)
#
#	Linker flags
#
XLIBS = -L/usr/X11R6/lib -lXmu -lX11 -lXmu -lXext -lXt
GL_LIBDIR = -L/home/gfx/lib/$(OSTYPE) 
GL_LIBS = $(GL_LIBDIR) -L/usr/local/lib -lglut -lGL -lGLU
VDSLIBS = -L$(VDSDIR) -lvds -lstdvds
LIBS = $(VDSLIBS) $(GL_LIBS) -lforms $(XLIBS) -lm 

OBJS = polyview.o polyviewctrl.o trackball.o polylex.o \
       poly.o geom.o invmatrix.o cluster.o

# Optimization/debuging flags.  Set to whatever level of optimization
# you are looking for:
# 	Debugging, no optimization
OPTFLAG = -g 
# 	Generic, portable optimization flag
# OPTFLAG = -O2
# 	Highest optimization level on SGIs.  Set ipXX to target CPU (run hinv)
# OPTFLAG = -Ofast=ip30

# Purify is a debugging tool for finding memory leaks. You shouldn't need this:
PURIFY = purify

# Suffix rules
.SUFFIXES: .o .c

.c.o :
	$(CC) $(CFLAGS) -c $<

# Rest of the rules.
default: polyview 
 
polyview: $(OBJS) ${VDSDIR}/libvds.a
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

purify: $(OBJS) ${VDSDIR}/libvds.a
	$(PURIFY) $(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

polylex.c: polylex.l
	flex -o$@ $<

clean:
	rm -f *.o polylex.c 

clobber: clean
	rm -f polyview polytest purify

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
