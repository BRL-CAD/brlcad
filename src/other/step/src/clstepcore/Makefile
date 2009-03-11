# $Id: Makefile,v 2.2.1.2 1997/11/05 21:59:12 sauderd DP3.1 $

# This code was developed with the support of the United States Government,
# and is not subject to copyright.

# This Makefile makes the library ../../arch/lib/libCedcore.a which is
#	made from the pdevel/src/clstepcore/ and pdevel/src/cleditor/
#	directories. 
# It does this by running the corresponding make in 
#	../../arch/ofiles/editorCore
# If you just type 'make' it will run 'make all'.
# It has the rule necessary for making a single object file.
# All object files are dropped in the directory named by the OBJDIR macro.

                X = X11
             MAKE = make X=$X
	   OBJDIR = ../../arch/ofiles/editorCore

	  LIBRARY = ../../arch/lib/libCedcore.a

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

#			make only the stepcore half of the library
stepcore::
	-@if [ -d $(OBJDIR) ]; then \
	echo "running make stepcore" "in $(OBJDIR)"; \
	cd $(OBJDIR); $(MAKE) stepcore; \
	else exit 0; fi	

#			make only the editor half of the library
editor::
	-@if [ -d $(OBJDIR) ]; then \
	echo "running make editor" "in $(OBJDIR)"; \
	cd $(OBJDIR); $(MAKE) editor; \
	else exit 0; fi	

#		This makes a single .o file.
#		It makes it by running make in $(OBJDIR), thus the
#			object file will end up in the $(OBJDIR) directory.
%.o:  %.cc
	-@if [ -d $(OBJDIR) ]; then \
	echo "running make" $< "in $(OBJDIR)"; \
	cd $(OBJDIR); $(MAKE) $@ ; \
	else exit 0; fi	
