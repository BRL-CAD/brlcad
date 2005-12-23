
# ------------------------------------------------------------------------
#
# 	Nmakefile for BLT library using VC++.
#
#  	Please note this file may or may not be up-to-date.  
#
#	You can compare it with "Makefile.vc" in this directory.  That's 
#	what I use to build BLT (so it should be current).  It builds BLT
#	with VC++ 6.0 and the cygwin32 tool suite from 
#
#		http://sourceware.cygnus.com
#
# ------------------------------------------------------------------------

!INCLUDE ../win/makedefs

TOOLS32       =	C:/Program Files/Microsoft Visual Studio/Vc98
prefix        =	C:/Program Files/Tcl

AR            =	lib.exe
LD            =	link.exe
CC            =	cl.exe
rc32          =	rc.exe
RM	      = -del

# ------------------------------------------------------------------------
# 	C Compiler options 
# ------------------------------------------------------------------------

DEFINES       =	-D_X86_=1 -D__STDC__ -DWIN32 -DCONSOLE -D_MT \
			$(DEBUG_DEFINES) $(SHLIB_DEFINES)
EXTRA_CFLAGS  =	-nologo -W3 

!IF "$(SHARED)" == "1"
SHLIB_DEFINES = -D_DLL
SHLIB_TARGET  =	build-dll
LIBS =		$(COMMON_LIBS) 
!ELSE
SHLIB_DEFINES = -D_CTYPE_DISABLE_MACROS
LIBS          =	$(COMMON_LIBS) $(EXTRA_LIBS)
!ENDIF

!IF "$(DEBUG)" == "1"
CFLAGS        =	-Z7 -Od
DEBUG_LDFLAGS =	-debug:full -debugtype:cv  
D             =	d
builddir      =	.\Debug
!ELSE
CFLAGS        =	-Ox -GB -GD 
DEBUG_LDFLAGS =	-debug:full -debugtype:cv  
D             =
builddir      =	.\Release
!ENDIF

MSVCRT        =	msvcrt$(DBG).lib
TK_LIB        =	$(TKDIR)/win/$(builddir)/tk$(v2)$(D).lib  
TCL_LIB       =	$(TCLDIR)/win/$(builddir)/tcl$(v2)$(D).lib 

# ------------------------------------------------------------------------
# 	Linker flags and options 
# ------------------------------------------------------------------------

JPEGLIB       =	$(JPEGDIR)/libjpeg.lib

COMMON_LDFLAGS =	-nodefaultlib -release -nologo -warn:3 \
		-machine:IX86 -align:0x1000 \
		$(DEBUG_LDFLAGS)

DLLENTRY      =	@12
SHLIB_LDFLAGS = $(COMMON_LDFLAGS) \
		-subsystem:console -entry:mainCRTStartup \
		-subsystem:windows -entry:WinMainCRTStartup \
		-entry:_DllMainCRTStartup$(DLLENTRY) -dll  

LDFLAGS       =	$(COMMON_LDFLAGS) \
		-fixed:NO -stack:2300000 

COMMON_LIBS   =	$(TK_LIB) $(TCL_LIB) \
		$(MSVCRT) \
		kernel32.lib user32.lib 

EXTRA_LIBS    =	$(OLELIB) \
		$(JPEGLIB) \
		gdi32.lib \
		oldnames.lib \
		advapi32.lib \
		winspool.lib 

TCL_ONLY_LIBS = $(TCL_LIB) $(MSVCRT)  kernel32.lib user32.lib advapi32.lib 

# ------------------------------------------------------------------------
# 	Source and target directories 
# ------------------------------------------------------------------------

srcdir        =	.
instdirs      =	$(prefix) $(exec_prefix) $(bindir) $(libdir) \
		$(includedir)
instdirs      =	$(exec_prefix) $(prefix) $(libdir)

# ------------------------------------------------------------------------
# 	Directories containing Tcl and Tk include files and libraries
# ------------------------------------------------------------------------

JPEGDIR       =	$(srcdir)/../../jpeg-6b
TCLDIR        =	$(srcdir)/../../tcl$(v3)
TKDIR         =	$(srcdir)/../../tk$(v3)
INCLUDES      =	-I. -I$(srcdir) \
		-I"$(TOOLS32)/include" \
		-I$(TCLDIR)/win \
		-I$(TCLDIR)/generic \
		-I$(TKDIR)/win \
		-I$(TKDIR)/generic \
		-I$(TKDIR)/xlib \
		-I$(JPEGDIR) 
SHLIB_LD_LIBS =	$(COMMON_LIBS) $(EXTRA_LIBS)

# ------------------------------------------------------------------------
# 	You don't need to edit anything beyond this point
# ------------------------------------------------------------------------

N_OBJS =	bltTed.o
V3_OBJS =	bltTri.o bltGrMt.o 

TK_OBJS =	tkButton.o tkFrame.o tkScrollbar.o 

GRAPH_OBJS =	bltGrAxis.o \
		bltGrBar.o \
		bltGrElem.o \
		bltGrGrid.o \
		bltGrHairs.o \
		bltGrLegd.o \
		bltGrLine.o \
		bltGrMarker.o \
		bltGrMisc.o \
		bltGrPen.o \
		bltGrPs.o \
		bltGraph.o 

TCL_ONLY_OBJS =	bltAlloc.o \
		bltArrayObj.o \
		bltBgexec.o \
		bltChain.o \
		bltDebug.o \
		bltHash.o \
		bltList.o \
		bltNsUtil.o \
		bltParse.o \
		bltPool.o \
		bltSpline.o \
		bltSwitch.o \
		bltTree.o \
		bltTreeCmd.o \
		bltUnixPipe.o \
		bltUtil.o \
		bltVector.o \
		bltVecMath.o \
		bltVecCmd.o \
		bltVecObjCmd.o \
		bltWatch.o  

OBJS =		$(GRAPH_OBJS) \
		$(TCL_ONLY_OBJS) \
		bltBeep.o \
		bltBind.o \
		bltBitmap.o \
		bltBusy.o \
		bltCanvEps.o \
		bltColor.o \
		bltConfig.o \
		bltContainer.o \
		bltCutbuffer.o \
		bltDragdrop.o \
		bltHierbox.o \
		bltHtext.o \
		bltImage.o \
		bltUnixImage.o \
		bltPs.o \
		bltTable.o \
		bltTabnotebook.o \
		bltTabset.o \
		bltText.o \
		bltTile.o \
		bltTreeView.o \
		bltTreeViewCmd.o \
		bltTreeViewEdit.o \
		bltTreeViewColumn.o \
		bltTreeViewStyle.o \
		bltUnixDnd.o \
		bltWindow.o \
		bltObjConfig.o \
		bltWinop.o \
		$(TK_OBJS) $(N_OBJS) 

bltwish =	bltwish.exe
bltsh =		bltsh.exe
headers =	$(srcdir)/blt.h \
		$(srcdir)/bltBind.h \
		$(srcdir)/bltChain.h \
		bltHash.h \
		$(srcdir)/bltList.h \
		$(srcdir)/bltPool.h \
		$(srcdir)/bltTree.h \
		$(srcdir)/bltVector.h 

version       =	$(BLT_MAJOR_VERSION)$(BLT_MINOR_VERSION)
bltwish2 =	bltwish$(version).exe
bltsh2 =	bltsh$(version).exe

lib_name =	BLT$(version)
lib_a =		BLT$(version).lib
lib_so =	BLT$(version).dll		
tcl_only_lib_a = BLTlite$(version).lib
tcl_only_lib_so = BLTlite$(version).dll		

CC_SWITCHES   =	$(CFLAGS) $(EXTRA_CFLAGS) $(DEFINES) $(INCLUDES)
VPATH         =	$(srcdir)

all: build-library $(SHLIB_TARGET) build-demos

build-demos: $(SHLIB_TARGET) $(bltwish) $(bltsh)

build-library: $(BLT_LIB)

build-library: $(lib_a) $(tcl_only_lib_a)

build-dll: build-library $(lib_so) $(tcl_only_lib_so)

$(bltwish): $(lib_a) tkConsole.o  bltWinMain.c
	$(RM) $(bltwish) 
	$(CC) -c $(CC_SWITCHES) -DTCLLIBPATH=\"$(TCLLIBPATH)\" \
		-FobltWinMain.o $(srcdir)/bltWinMain.c
	LIB=$(TOOLS32)/lib \
	$(LD) $(LDFLAGS) tkConsole.o bltWinMain.o -out:$(bltwish) \
		$(lib_a) $(LIBS) 

$(bltsh): $(tcl_only_lib_a) bltWinMain.c
	$(RM) $(bltsh) 
	$(CC) -c $(CC_SWITCHES) -DTCL_ONLY \
		-DTCLLIBPATH=\"$(TCLLIBPATH)\" \
		-FobltWinMain.o $(srcdir)/bltWinMain.c
	LIB=$(TOOLS32)/lib \
	$(LD) $(LDFLAGS) bltWinMain.o -out:$(bltsh) \
		$(tcl_only_lib_a) $(TCL_ONLY_LIBS) 

$(lib_a):  bltHash.h $(OBJS) bltInit.c
	$(RM) bltInit.o
	$(CC) -c $(CC_SWITCHES)  -DBLT_LIBRARY=\"$(BLT_LIBRARY)\" \
		-FobltInit.o $(srcdir)/bltInit.c
	$(RM) $@
	$(AR) -out:$@ bltInit.o $(OBJS)

$(lib_so): $(lib_a) $(OBJS) bltInit.c
	$(RM) bltInit.o
	$(CC) -c $(CC_SWITCHES) -DBLT_LIBRARY=\"$(BLT_LIBRARY)\" \
		-FobltInit.o $(srcdir)/bltInit.c
	$(RM) $@
	LIB=$(TOOLS32)/lib \
	$(LD) $(SHLIB_LDFLAGS) -out:$@ bltInit.o $(OBJS) $(SHLIB_LD_LIBS)

$(tcl_only_lib_a):  bltHash.h $(TCL_ONLY_OBJS) bltInit.c
	$(RM) bltInit.o
	$(CC) -c $(CC_SWITCHES) -DTCL_ONLY -DBLT_LIBRARY=\"$(BLT_LIBRARY)\" \
		-FobltInit.o $(srcdir)/bltInit.c
	$(RM) $@
	$(AR) -out:$@ bltInit.o $(TCL_ONLY_OBJS) 

$(tcl_only_lib_so): $(tcl_only_lib_a) $(TCL_ONLY_OBJS) bltInit.c
	$(RM) bltInit.o
	$(CC) -c $(CC_SWITCHES) -DTCL_ONLY -DBLT_LIBRARY=\"$(BLT_LIBRARY)\" \
		-FobltInit.o $(srcdir)/bltInit.c
	$(RM) $@
	LIB=$(TOOLS32)/lib \
	$(LD) $(SHLIB_LDFLAGS) -out:$@ bltInit.o $(TCL_ONLY_OBJS) \
		$(TCL_ONLY_LIBS) 

bltHash.h: bltHash.h.in
	sed -e 's/@SIZEOF_VOID_P@/4/' \
	    -e 's/@SIZEOF_INT@/4/' \
	    -e 's/@SIZEOF_LONG@/4/' \
	    -e 's/@SIZEOF_LONG_LONG@/8/' \
	    -e 's/@HAVE_INTTYPES_H@/0/' \
	    bltHash.h.in > bltHash.h

clean:
	-del *.o 2>nul
	-del *.pdb 2>nul
	-del *.exp 2>nul
	-del $(lib_name).* 2>nul
	-del $(bltwish) 2>nul
	-del $(bltsh) 2>nul
	-del $(srcdir)\*.bak 2>nul
	-del $(srcdir)\*~ 2>nul 
	-del $(srcdir)\"#"* 2>nul

{$(srcdir)}.c.o:
	$(CC) -c $(CC_SWITCHES) -Fo$*.o $<









