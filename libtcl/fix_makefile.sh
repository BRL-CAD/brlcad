#!/bin/sh

sed -e s@lib/tcl\$\(VERSION\)@tclscripts@ Makefile |\
	 sed -e s@/\(prefix\)/include@/\(prefix\)/include/brlcad@  |\
	 sed -e "s@\${\C\C}  \${TCLSH_OBJS}@\${\C\C} $LDFLAGS \${TCLSH_OBJS}@" |\
	 sed -e "s@\${\C\C}  \${TCLTEST_OBJS}@\${\C\C} $LDFLAGS \${TCLTEST_OBJS}@" |\
	 sed -e s@tcl8.0@tcl@ |\
	 sed -e s@tcl80@tcl@
