# $Id: Makefile,v 2.2.1.1 1997/11/05 23:05:42 sauderd DP3.1 $

# This code was developed with the support of the United States Government,
# and is not subject to copyright.

# This Makefile makes the library ../../arch/lib/libCivfasd.a which is
#	made from the pdevel/src/clivfasd/ directory. 
# It does this by running the corresponding make in 
#	../../arch/ofiles/ivfasd
# If you just type 'make' it will run 'make all'.
# It has the rule necessary for making a single object file.
# All object files are dropped in the directory named by the OBJDIR macro.

	       CC = g++
                X = X11
             MAKE = make X=$X
	   OBJDIR = ../../arch/ofiles/ivfasd

	  LIBRARY = ../../arch/lib/libCivfasd.a

.KEEP_STATE:
.SUFFIXES: . .o .c .h .cc

#			make the whole library
all: $(LIBRARY)

#			make the whole library
$(LIBRARY)::
	-@if [ -d $(OBJDIR) ]; then \
	echo "running make all" "in $(OBJDIR)"; \
	cd $(OBJDIR); $(MAKE) all; \
	else exit 0; fi	

#		This makes a single .o file.
#		It makes it by running make in $(OBJDIR), thus the
#			object file will end up in the $(OBJDIR) directory.
%.o:  %.cc
	-@if [ -d $(OBJDIR) ]; then \
	echo "running make" $< "in $(OBJDIR)"; \
	cd $(OBJDIR); $(MAKE) $@ ; \
	else exit 0; fi	
