# ------------------------------------------------------------------------
#       Makefile for demonstation shell of BLT library
# ------------------------------------------------------------------------

!INCLUDE ./win/makedefs

# ------------------------------------------------------------------------
#       Source and target installation directories
# ------------------------------------------------------------------------

srcdir		= .

# ------------------------------------------------------------------------
#       Don't edit anything beyond this point
# ------------------------------------------------------------------------

all:  
	cd $(MAKEDIR)\src
	$(MAKE) -f blt.mak all
	cd $(MAKEDIR)

install: install-all

install-all:
 	wish$(v2)d.exe win/install.tcl $(v1) $(srcdir)

clean:
	cd $(MAKEDIR)\src
	$(MAKE) -f blt.mak clean
	cd $(MAKEDIR)
	$(RM) *.bak *\~ "#"* *pure* .pure*

distclean: clean

