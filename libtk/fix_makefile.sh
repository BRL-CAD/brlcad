#!/bin/sh

sed -e s@lib/tk\$\(VERSION\)@tclscripts@ Makefile |\
	 sed -e s@\(prefix\)/include@\(prefix\)/include/brlcad@ |\
	 sed -e "s@\$(\C\C)  \$(WISH_OBJS)@\$(\C\C) $LDFLAGS \$(WISH_OBJS)@" |\
	 sed -e "s@\${\C\C}  \$(TKTEST_OBJS)@\${\C\C} $LDFLAGS \$(TKTEST_OBJS)@" |\
	 sed -e s@tk8.0@tk@ | sed -e s@tcl8.0@tcl@ |\
	 sed -e s@tk80.so.1.0@tk.so@ | sed -e s@tcl80.so.1.0@tcl.so@ |\
	 sed -e s@tk80@tk@ | sed s@tcl80@tcl@
