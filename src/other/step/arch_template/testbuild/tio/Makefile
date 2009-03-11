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

SRCS = $(TEST_SRC)/tio/tio.cc

OFILES = tio.o ../needFunc.o

%.o:  $(TEST_SRC)/tio/%.cc
	@echo " "
	@echo "compiling $<";
	$(CXX) $(CFLAGS) $(INCLUDES) -c $<

all:  tio

tio: $(OFILES)
	$(CXX) -o tio $(OFILES) $(SCL_LIBS) $(LDFLAGS);
 
#ocenter_src:    $(SRCS) *.h
#        #load $(CFLAGS) $(SRCS)

#ocenter_obj:    $(OBJS)
#        #load $(CFLAGS) $(OFILES)

clean:
	touch tio foo.o core;
	rm tio *.o core;
	
depend: Makefile $(SRCS)
	$(MAKE_DEPEND) $(INCLUDES) $(CPLUSINCLUDES) $(COND_DEFS) $(SRCS);


# DO NOT DELETE THIS LINE -- make depend depends on it.
