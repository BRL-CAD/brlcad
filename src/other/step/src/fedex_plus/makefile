# $Id: makefile,v 3.0.1.8 1998/01/07 15:28:26 dar Exp $
#
# Makefile for fedex_plus
# Developed for US Government; not subject to copyright

# Pick up default macros and rules
include ../express/mkrules

################################
# User-definable flags to CC:
# Put whatever you want in here!
################################
# Uncomment this if you want info printed on the structure of the
# complex entities found in the EXPRESS file:
#USER_DEFS = -DCOMPLEX_INFO

# following only on Sparcs
SABERDEFINE = -D__sparc__

################################
# User-definable flags to makedepend
################################
COND_DEFS = -D__STDC__

################################
# Default rule to compile C source files
#  this is standard - doesn't need explicit
################################

#.c.o:
#	$(CC) $(CFLAGS) -c $*.c

%.o: %.cc
	$(CXX) -g $(CFLAGS) $(USER_DEFS) $(INCLUDES) -c $<

%.o: %.c
	$(CC) -g $(CFLAGS) $(USER_DEFS) $(INCLUDES) -c $<

################################################################
#  Library files
################################################################


################################################################
#  Object files
################################################################

OFILES = fedex_main.o classes_misc.o classes_wrapper.o classes.o selects.o \
         multpass.o collect.o complexlist.o entlist.o multlist.o orlist.o \
         entnode.o expressbuild.o non-ors.o match-ors.o trynext.o \
         write.o print.o


################################################
#  Source files
################################################

CFILES = fedex_main.c classes_misc.c classes_wrapper.cc classes.c selects.c \
	 multpass.c collect.cc complexlist.cc entlist.cc multlist.cc orlist.cc \
         entnode.cc expressbuild.cc non-ors.cc match-ors.cc trynext.cc \
         write.cc print.cc fedex_idl.c


################################################################
#  Executable to build
################################################################

PROG = fedex_plus

################################################################
# Here's the rule that builds the executables.
################################################################

all: fedex_plus fedex_os fedex_idl

$(PROG): $(OFILES) $(PDESLIBDIR)libexpress.a $(PDESLIBDIR)libexppp.a
	 $(CXX) $(CFLAGS) $(INCLUDES) $(OFILES) $(LDFLAGS) -L$(PDESLIBDIR) \
	 -lexppp -lexpress -o $(PROG)

fedex_os: fedex_os.o classes_misc.o
	$(CC) $(CFLAGS) -o fedex_os fedex_os.o classes_misc.o $(LDFLAGS) \
	-L$(PDESLIBDIR) -lexppp -lexpress

fedex_idl: fedex_idl.o classes_misc.o
	$(CC) $(CFLAGS) -o fedex_idl fedex_idl.o classes_misc.o $(LDFLAGS) \
	-L$(PDESLIBDIR) -lexppp -lexpress

relink: 
	 $(CC) $(CFLAGS) -o $(PROG) $(OFILES) $(LIBS) 

################################################################
# Put any rules for building your object files here.
################################################################

install : fedex_os_install fedex_idl_install fedex_plus_install

fedex_plus_install : fedex_plus
	mv fedex_plus $(PDESBINDIR)

fedex_os_install : fedex_os
	mv fedex_os $(PDESBINDIR)

fedex_idl_install : fedex_idl
	mv fedex_idl $(PDESBINDIR)

clean	:
	rm -f *.o

depend	:
	makedepend $(COND_DEFS) $(CFLAGS) $(CFILES)


################################
# Saber-C rules
################################

saber_src: $(CFILES)
	#load $(CFLAGS) $(CFILES) $(LIBS)

saber_obj: $(OFILES)
	#load $(OFILES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
