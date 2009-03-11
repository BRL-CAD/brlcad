# This Makefile is a wrapper for all the Makefiles for the individual
# test programs.  Just typing 'make' in this directory will build all
# of the programs.  To build them individually, either run make from their
# own directory, or from here type 'make progname', where progname is the
# program you want to build.

.KEEP_STATE:

.SUFFIXES: . .o .c .h .cc

TESTS = p21read tstatic treg tio scl2html

%.h:

HEADERS = tests.h SEarritr.h

include schema-loc

INCLUDES =  $(CC_INCLUDES) -I.

%.o:  %.cc
	@echo " "
	@echo "compiling $<";
	$(CC) $(CFLAGS) $(INCLUDES) -c $< 

all:  needFunc.o $(TESTS) 

p21read::
	   cd p21read; make all

tstatic::
	   cd tstatic; make all

treg::
	   cd treg; make all

tio::
	   cd tio; make all

scl2html::
	   cd scl2html; make all

clean:
	touch foo.o; rm *.o;
	cd p21read; make clean;
	cd tstatic; make clean;
	cd treg; make clean;
	cd tio; make clean;
	cd scl2html; make clean;
	
depend: 
	cd p21read; make depend
	cd tstatic; make depend
	cd treg; make depend
	cd tio; make depend
	cd scl2html; make depend

