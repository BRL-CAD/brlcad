# Include information about directory structure... may need editing!
include ../schema-loc

           SCL_ED = $(PDES_ROOT)/src/cleditor
             CORE = $(PDES_ROOT)/src/clstepcore
            UTILS = $(PDES_ROOT)/src/clutils
	      DAI = $(PDES_ROOT)/src/cldai

     PDESINCLUDES = -I$(SCL_ED) \
		    -I$(CORE) \
		    -I$(UTILS) \
		    -I$(DAI) \
		    -I$(SCHEMA_DIR)

         INCLUDES = -I. -I..\
		    $(PDESINCLUDES) \
		    $(CC_INCLUDES)

	  LIBPDES = \
		    -L$(ARCH_ROOT)/lib \
		    -lCedcore \
		    -lCutils \
		    -lCdai

         SCL_LIBS = $(LIBSCHEMA) $(LIBPDES) 

.KEEP_STATE:

.SUFFIXES: . .o .c .h .cc

SRCS = tstatic.cc ../needFunc.o

OFILES = tstatic.o ../needFunc.o

%.o:  %.cc
	@echo " "
	@echo "compiling $<";
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

all: tstatic

tstatic: $(OFILES)
	$(CC) -o tstatic $(OFILES) $(SCL_LIBS) $(LDFLAGS);
 
ocenter_src:    $(SRCS) *.h
        #load $(CFLAGS) $(SRCS)

ocenter_obj:    $(OBJS)
        #load $(CFLAGS) $(OFILES)

clean:
	touch tstatic foo.o core;
	rm tstatic *.o core;
	
depend: Makefile $(SRCS)
	$(MAKE_DEPEND) $(INCLUDES) $(COND_DEFS) $(SRCS);


# DO NOT DELETE THIS LINE -- make depend depends on it.

