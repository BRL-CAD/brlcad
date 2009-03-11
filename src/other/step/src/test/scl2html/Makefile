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

SRCS = scl2html.cc ../needFunc.o

OFILES = scl2html.o ../needFunc.o

%.o:  %.cc
	@echo " "
	@echo "compiling $<";
	$(CC) $(CFLAGS) $(INCLUDES) -c $< 

all:  scl2html

scl2html: $(OFILES)
	$(CC) -o scl2html.$(SCHEMA_NAME) $(OFILES) $(SCL_LIBS) $(LDFLAGS);
	strip scl2html.$(SCHEMA_NAME);

scl2html.tc: $(OFILES)
	proof $(CC) -g -o scl2html.$(SCHEMA_NAME).tc \
		$(OFILES) $(SCL_LIBS) $(LDFLAGS)

clean:
	touch scl2html.$(SCHEMA_NAME) foo.tc foo.o core;
	rm scl2html.$(SCHEMA_NAME) *.tc *.o core;
	
depend: Makefile $(SRCS)
	$(MAKE_DEPEND) $(INCLUDES) $(COND_DEFS) $(SRCS);


# DO NOT DELETE THIS LINE -- make depend depends on it.

