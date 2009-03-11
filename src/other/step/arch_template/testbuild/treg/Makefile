# Include information about directory structure... may need editing!
include ../schema-loc

TEST_SRC=../../../src/test

SCL_ED = $(PDES_ROOT)/src/cleditor
CORE = $(PDES_ROOT)/src/clstepcore
UTILS = $(PDES_ROOT)/src/clutils
DAI = $(PDES_ROOT)/src/cldai

PDESINCLUDES = -I$(SCL_ED) \
	    -I$(CORE) \
	    -I$(UTILS) \
	    -I$(DAI) \
	    -I$(SCHEMA_DIR) \
	    -I$(ARCH_ROOT)

INCLUDES = -I. \
	-I$(TEST_SRC) \
	$(MR_INCLUDES) \
	$(PDESINCLUDES)

LIBPDES = \
    -L$(ARCH_ROOT)/lib \
    -lCedcore \
    -lCutils \
    -lCdai

SCL_LIBS = $(LIBSCHEMA) $(LIBPDES) 

.KEEP_STATE:

.SUFFIXES: . .o .c .h .cc

SRCS = $(TEST_SRC)/treg/treg.cc

OFILES = treg.o ../needFunc.o

%.o:  $(TEST_SRC)/treg/%.cc
	@echo " "
	@echo "compiling $<";
	$(CXX) $(CFLAGS) $(INCLUDES) -c $<

all:  treg

treg: $(OFILES)
	$(CXX) -o treg $(OFILES) $(SCL_LIBS) $(LDFLAGS);
 
#ocenter_src:    $(SRCS) *.h
#        #load $(CFLAGS) $(SRCS)

#ocenter_obj:    $(OBJS)
#        #load $(CFLAGS) $(OFILES)

clean:
	touch treg foo.o core;
	rm treg *.o core;
	
depend: Makefile $(SRCS)
	$(MAKE_DEPEND) $(INCLUDES) $(CPLUSINCLUDES) $(COND_DEFS) $(SRCS);


# DO NOT DELETE THIS LINE -- make depend depends on it.
