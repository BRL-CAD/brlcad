# Makefile for exppp (NIST Express Pretty Printer)
# Developed for US Government; not subject to copyright

################################################################
#
# Per-site configuration stuff
#
# Notes:
#    Due to an optimization bug in GCC (on Sun3, no fpa), expparse.c
#	must be compiled without optimization.  Hence, the explicit
#	rule.
#
################################################################

include ../express/mkrules

################################
# Configuration options
################################

# following defns necessary only if you are using Centerline,
# else comment out
GCCROOT = /depot/gnu/arch/lib/gcc-lib/sparc-sun-sunos4.1/2.4.5
GCCLIB = $(GCCROOT)/libgcc.a
GCCINC = -I$(GCCROOT)/include
# following only on Sparcs
SABERDEFINE = -D__sparc__

# CFLAGS options are as follows:
# Some (as noted) are not optional (yeah, mandatory options):
#
#EXPRESS_CFLAGS = -I. -I$(EXPINCDIR) -I/depot/gnu/arch/lib/gcc-include

# DAR - commented out because the only things add'l to the one in mkrules
# are the flags which we don't want (the last 2):
#EXPRESS_CFLAGS = -I. -I$(EXPINCDIR) $(GCCINC) $(SABERDEFINE)
#    $(EXPRESS_CFLAGS) should always be in CFLAGS.  It usually allows some
#    to be inherited from make_rules.  Set it here to temporarily override
#    the definition in make_rules.
#
#-DFLEX_DEBUG
#
#    This software includes a memory allocator which runs faster and uses less
#    space than malloc, however it is harder to debug problems that corrupt
#    memory.  So you have the option of using the regular malloc or not.  To
#    use the real malloc, uncomment the following line and recompile memory.c
MALLOC = -DREAL_MALLOC
#    to debug real malloc, uncomment the following as well
#DEBUG_MALLOC = -DDEBUG_MALLOC
#DEBUG_MALLOC_LIB = /usr/lib/debug/malloc.o
#
#-g turns on debugging.
#-pg turns on profiling suitable for gprof.
#
# -DHASH_DEBUG and -DHASH_STATISTICS do what their names imply.  See hash.c
#    for more info.
#
#-DYYDEBUG can be added to allow parser debugging.  Note that this just
#    controls addition of code.  Actual running of code must be enabled by
#    setting yydebug either by hand or through command-line argument.
#
CFLAGS = -g $(MALLOC) $(DEBUG_MALLOC) -DYYDEBUG -D$(SCANNER) \
         $(EXPRESS_CFLAGS) $(COND_DEFS)

OPTIMIZE = 
#OPTIMIZE = -O -finline-functions -fstrength-reduce 

#-d for debug
#-s for catching unmatched characters
#-v for summary statistics
LOCAL_FLEXFLAGS = -CF -L
# lex -c dumps core!
LOCAL_LEXFLAGS = -c

#YFLAGS = -tv
#YFLAGS = -t 
#YFLAGS = 

EXPPARSE_DIRT = expparse.tok*.h expparse.h expparse.c expparse.tab.output 

MIC = $(UTIL)/move-if-change

################################################
# Object files
################################################

# object files with their own build rules
#OFILES1 = symbol.o expparse.o expscan.o lex_actions.o express.o
# was construct.o

# object files for the generic build rule
#OFILES2 = algorithm.o case_item.o entity.o expression.o fedex.o init_hook.o resolve.o schema.o scope.o statement.o type.o variable.o
# constant.o

# all object files
OFILES = $(OFILES1) $(OFILES2) exppp.o exppp-main.o

################################################
# Source files
################################################

C_SOURCES = exppp-main.c exppp.c
# used to include construct.c constant.c
#OTHER_SOURCES = fedex.c expparse.y expscan.l

SOURCES = $(C_SOURCES) $(OTHER_SOURCES)

################################################
# Header files for Express library
################################################

#HFILES = $(EXPINCDIR)symbol.h $(EXPINCDIR)type.h $(EXPINCDIR)variable.h $(EXPINCDIR)expression.h $(EXPINCDIR)entity.h $(EXPINCDIR)case_item.h $(EXPINCDIR)statement.h $(EXPINCDIR)algorithm.h $(EXPINCDIR)scope.h $(EXPINCDIR)schema.h $(EXPINCDIR)express.h $(EXPINCDIR)resolve.h $(EXPINCDIR)express_basic.h $(EXPINCDIR)lex_actions.h 

################################################
# Header files for libmisc modules
################################################

#MISC_HFILES = $(EXPINCDIR)basic.h $(EXPINCDIR)dictionary.h $(EXPINCDIR)error.h $(EXPINCDIR)hash.h $(EXPINCDIR)linked_list.h $(EXPINCDIR)memory.h $(EXPINCDIR)stack.h $(EXPINCDIR)object.h

################################
# Doc files for distribution
################################

# DOCS = fedex.ps fedex-reference.ps 

################################
# This target (the default) builds the library and
# all associated stuff.
################################

all: libexppp.a exppp

exppp:	$(OFILES)
#	$(CC) $(CFLAGS) $(OFILES) $(DEBUG_MALLOC_LIB) -o exppp -L$(PDESLIBDIR) -lbison -ll
	$(CC) $(CFLAGS) $(OFILES) $(DEBUG_MALLOC_LIB) -o exppp $(EXPRESS_LIBS)

################################
# Build the library.
################################

libexppp.a: $(OFILES) 
	$(AR) rc libexppp.a $?
	$(RANLIB) libexppp.a

################################
# Build and install everything.
################################

install: all force
	-mkdir -p $(PDESLIBDIR)
	-mkdir -p $(PDESBINDIR)
	cp exppp $(PDESBINDIR)
	cp libexppp.a $(PDESLIBDIR)
	cd $(PDESLIBDIR); $(RANLIB) libexppp.a

################################
# Start with a clean slate
################################

clean: force
	rm -f *.~*~ *.o libexppp.a

################################
# Rules for object files
################################

# comment out because OFILES2 is empty!
# $(OFILES2): $(HFILES) $(MISC_HFILES)

symbol.o: symbol.c $(EXPINCDIR)symbol.h $(MISC_HFILES)
express.o: express.c $(EXPINCDIR)express.h $(EXPINCDIR)resolve.h $(EXPINCDIR)stack.h
construct.o: construct.c $(EXPINCDIR)construct.h $(MISC_HFILES)

################################
# Build the actual parser
################################

expparse.o: expparse.c $(HFILES) $(MISC_HFILES)
	$(CC) $(CFLAGS_I) $(CFLAGS) -c expparse.c -DYYMAXDEPTH=1000

################################
# Build the lexical analyzer
################################

expscan.o: expscan.c $(HFILES) $(MISC_HFILES) expparse.tokens.h
	$(CC) -traditional $(CCFLAGS) -c $*.c

################################
# Build the working routines for the lexical analyzer
################################

lex_actions.o: lex_actions.c $(HFILES) $(MISC_HFILES) expparse.h

################################
# Dummy target to force execution of actions
################################

force:	;

################################
# expparse.tokens.h contains #defines for all tokens returned by yylex.
################################

expparse.tokens.h: expparse.h
	$(GREP) "# *define" expparse.h > .expparse.h
	$(MIC) .expparse.h expparse.tokens.h

#
# Push out to ftp directory
#

exppp.tar:
	cd ..;tar cvf exppp.tar `/proj/pdevel/bin/pubfile exppp`
	mv ../exppp.tar .

exppp.tar.Z: exppp.tar
	rm -f exppp.tar.Z
	compress exppp.tar

FTPDIR = /proj/elib/online/pub/step/npttools
#FTPDIR = /proj/pdevel/tmp

ftp:	exppp.tar.Z
	cp exppp.tar.Z $(FTPDIR)
	rm exppp.tar.Z

exppp.exe.Z: exppp.exe
	compress exppp.exe

dosftp:	exppp.exe.Z
	cp exppp.exe.Z $(FTPDIR)

################################
# Centerline rules
################################

saber_src: $(C_SOURCES)
	#load $(CFLAGS) $(C_SOURCES) $(GCCLIB) -L$(PDESLIBDIR) -lexpress -lbison -ll

# This definition skips main
saber_src2: $(C_SOURCES) expparse.c expscan.c express.c
	#suppress 592 rcsid
	#suppress 592 _rcsid
	#suppress 592 ncform_sccsid
	#suppress 593 yyerrlab1
	#suppress 593 yybackup
	#suppress 593 yyfussy
	#load $(CFLAGS) $(C_SOURCES) expparse.c expscan.c express.c $(GCCLIB) -L$(PDESLIBDIR) -lbison -ll

saber_obj: $(OFILES)
	#load $(OFILES)

