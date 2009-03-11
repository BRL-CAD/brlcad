# $Id: makefile,v 1.7 1997/10/22 16:41:15 sauderd Exp $
#
# Makefile for Fed-X Express Parser and library
# Developed for US Government; not subject to copyright
#
# $Log: makefile,v $
# Revision 1.7  1997/10/22 16:41:15  sauderd
# Commented out EXPRESS_CFLAGS since it was only supposed to temporarily
# override the one in mkrules (according to the comment next to it). Anyway
# it was also causing problems.
#
# Revision 1.6  1997/10/16 20:40:44  sauderd
# Changed the ordering of the make rules so that it builds all by default.
# Changed the install rule so that it finishes copying before it cds
#
# Revision 1.5  1997/09/29 17:51:41  dar
# Updated to work with the new mkrules.xxx files.  Also, removed
# the generated dependencies (using make depend) since they depend
# on the current mkrules and should not be hard wired.
#
# Revision 1.5  1997/09/23 20:26:15  dar
# Updated to work with the new mkrules.xxx files.  Also, removed
# the generated dependencies (using make depend) since they depend
# on the current mkrules and should not be hard wired.
#
# Revision 1.4  1997/01/21  19:55:30  dar
# made C++ compatible
#
# Revision 1.2  1994/11/10 19:20:03  clark
# Update to IS
#
# Revision 1.1  1994/05/11  19:50:00  libes
# Initial revision
#
# Revision 1.10  1993/10/29  15:56:05  libes
# *** empty log message ***
#
# Revision 1.9  1993/10/15  18:47:26  libes
# CADDETC certified
#
# Revision 1.7  1993/03/22  18:06:52  libes
# added symlink
#
# Revision 1.6  1993/02/22  21:40:55  libes
# added strdup
#
# Revision 1.5  1993/02/16  03:13:30  libes
# match doc
#
# Revision 1.4  1993/01/19  22:45:07  libes
# *** empty log message ***
#
# Revision 1.3  1992/08/18  18:27:25  libes
# improved comments in Makefile
#
# Revision 1.2  1992/08/18  17:11:36  libes
# rm'd extraneous error messages
#
# Revision 1.1  1992/05/28  03:52:25  libes
# Initial revision

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

include mkrules

INCLUDES = -I. -I$(CPLUS_INC_DIR)

################################
# Configuration options
################################

# CFLAGS options are as follows:
# Some (as noted) are not optional (yeah, mandatory options):
#
#EXPRESS_CFLAGS = -I.
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
# Object files for Express library
################################################

# object files with their own build rules
OFILES1 = symbol.o expparse.o expscan.o lexact.o express.o

# object files for the generic build rule
OFILES2 = alg.o caseitem.o entity.o expr.o inithook.o resolve.o schema.o scope.o stmt.o type.o variable.o yyvars.o

# all object files for libexpress.a
OFILES = $(OFILES1) $(OFILES2) linklist.o error.o dict.o hash.o memory.o object.o fedex.o

################################################
# Source files for Express library
################################################

C_SOURCES = symbol.c type.c variable.c expr.c entity.c caseitem.c stmt.c alg.c scope.c schema.c resolve.c lexact.c linklist.c error.c dict.c hash.c memory.c object.c inithook.c
OTHER_SOURCES = fedex.c expparse.y expscan.l

SOURCES = $(C_SOURCES) $(OTHER_SOURCES)

################################################
# Header files for Express library
################################################

HFILES = $(EXPINCDIR)symbol.h $(EXPINCDIR)type.h $(EXPINCDIR)variable.h $(EXPINCDIR)expr.h $(EXPINCDIR)entity.h $(EXPINCDIR)caseitem.h $(EXPINCDIR)stmt.h $(EXPINCDIR)alg.h $(EXPINCDIR)scope.h $(EXPINCDIR)schema.h $(EXPINCDIR)express.h $(EXPINCDIR)resolve.h $(EXPINCDIR)expbasic.h $(EXPINCDIR)lexact.h 

################################################
# Header files for libmisc modules
################################################

MISC_HFILES = $(EXPINCDIR)basic.h $(EXPINCDIR)dict.h $(EXPINCDIR)error.h $(EXPINCDIR)hash.h $(EXPINCDIR)linklist.h $(EXPINCDIR)memory.h $(EXPINCDIR)stack.h $(EXPINCDIR)object.h

################################
# Doc files for distribution
################################

# DOCS = fedex.ps fedex-reference.ps 

################################
# This target (the default) builds the library and
# all associated stuff.
################################

all: libexpress.a fedex symlink

fedex:  $(OFILES)
	$(CC) $(CFLAGS) $(OFILES) $(DEBUG_MALLOC_LIB) -o fedex

# This would work except that the library has not been installed yet when
# this rule is run (when making all)
#	$(CC) $(CFLAGS) $(DEBUG_MALLOC_LIB) -L$(PDESLIBDIR) $(EXPLIB) -o fedex

dosfedex: $(OFILES) libdos.a
	$(CC) -o fedex fedex.o libexpress.a
	aout2exe fedex

symlink: symlink.o libexpress.a
	$(CC) $(CFLAGS) symlink.o libexpress.a -o symlink -L$(PDESLIBDIR)

################################
# Build the library.
################################

libexpress.a: $(OFILES) 
	$(AR) rc libexpress.a $(OFILES)
	$(RANLIB) libexpress.a

# for dos
libdos.a: $(OFILES) pause.o
	$(AR) rc libexpress.a *.o
	$(RANLIB) libexpress.a
# force libdos.a to exist so that the rule won't be reinvoked unnecessarily
# each time fedex is build.  "touch" doesn't exist on the pc, so just copy
# over some small file.
	copy pause.o libdos.a

################################
# Build and install everything.
################################

install: all force
	-mkdir -p $(PDESLIBDIR)
	-mkdir -p $(PDESBINDIR)
	cp fedex $(PDESBINDIR)
	cp symlink $(PDESBINDIR)
	cp libexpress.a $(PDESLIBDIR)
	cd $(PDESLIBDIR); $(RANLIB) libexpress.a

################################
# Start with a clean slate
################################

clean: force
	rm -f *.~*~ *.o libexpress.a $(EXPPARSE_DIRT) expscan.c 

################################
# Rules for object files
################################

$(OFILES2) fedex.o: $(HFILES) $(MISC_HFILES)

symbol.o: symbol.c $(EXPINCDIR)symbol.h $(MISC_HFILES)
express.o: express.c $(EXPINCDIR)express.h $(EXPINCDIR)resolve.h $(EXPINCDIR)stack.h

################################
# Build the actual parser
################################

expparse.o: expparse.c $(HFILES) $(MISC_HFILES)
	/bin/rm -f $*.o
	$(CC) $(CFLAGS_I) $(CFLAGS) -c expparse.c -DYYMAXDEPTH=1000

################################
# Build the lexical analyzer
################################

expscan.o: expscan.c $(HFILES) $(MISC_HFILES) exptoks.h
	/bin/rm -f $*.o
	$(CC) $(CCFLAGS) -c $*.c

#	$(CC) -traditional $(CCFLAGS) -c $*.c

################################
# Build the working routines for the lexical analyzer
################################

lexact.o: lexact.c $(HFILES) $(MISC_HFILES) expparse.h

################################
# Dummy target to force execution of actions
################################

force:	;

################################
# exptoks.h contains #defines for all tokens returned by yylex.
################################

#expparse.tokens.h: expparse.h
#	$(GREP) "# *define" expparse.h > .expparse.h
#	$(MIC) .expparse.h expparse.tokens.h

exptoks.h: expparse.h
	$(GREP) "# *define" expparse.h > exptmp.h
	$(MIC) exptmp.h exptoks.h


#
# Push out to ftp directory
#

exptk.tar:
	cd ../..;tar cvf exptk.tar `/proj/pdevel/bin/pubfile src/express` \
				   `/proj/pdevel/bin/pubfile bin`
	mv ../../exptk.tar .

exptk.tar.Z: exptk.tar
	rm -f exptk.tar.Z
	compress exptk.tar

FTPDIR = /proj/elib/online/pub/step/npttools
#FTPDIR = /proj/pdevel/tmp

ftp:	exptk.tar.Z
	cp exptk.tar.Z $(FTPDIR)
	rm exptk.tar.Z

fedex.exe.Z: fedex.exe
	compress fedex.exe

dosftp:	fedex.exe.Z
	cp fedex.exe.Z $(FTPDIR)

################################
# Saber-C rules
################################

saber_src: $(C_SOURCES) expparse.c expscan.c express.c
	#load $(CFLAGS) $(C_SOURCES) fedex.c expparse.c expscan.c express.c -L$(PDESLIBDIR) -ll

# This definition skips main
saber_src2: $(C_SOURCES) expparse.c expscan.c express.c
	#suppress 592 rcsid
	#suppress 592 _rcsid
	#suppress 592 ncform_sccsid
	#suppress 593 yyerrlab1
	#suppress 593 yybackup
	#suppress 593 yyfussy
	#load $(CFLAGS) $(C_SOURCES) expparse.c expscan.c express.c -L$(PDESLIBDIR) -ll

saber_obj: $(OFILES) fedex.o
	#load $(OFILES) fedex.o

# DAR - Note - I had troubles with makedepend because my path used -
# /depot/openwin3/bin/makedepend
# which ignored #ifdef's, while this (newer) one worked properly -
# /depot/x11r5/plat/bin/makedepend

depend:
	makedepend $(CFLAGS) $(CFILES) $(C_SOURCES)
# DO NOT DELETE THIS LINE -- make depend depends on it.

