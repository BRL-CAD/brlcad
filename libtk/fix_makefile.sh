#!/bin/sh

sed -e s@lib/tk\$\(VERSION\)@tk\$\(VERSION\)@ Makefile |\
	 sed -e s@\(prefix\)/include@\(prefix\)/include/brlcad@ |\
	 sed -e "s@\$(\C\C)  \$(WISH_OBJS)@\$(\C\C) $LDFLAGS \$(WISH_OBJS)@" |\
	 sed -e "s@\${\C\C}  \$(TKTEST_OBJS)@\${\C\C} $LDFLAGS \$(TKTEST_OBJS)@" |\
	 sed -e s@tk8.2@tk@ | sed -e s@tcl8.2@tcl@ |\
	 sed -e s@tk82.so.1.0@tk.so@ | sed -e s@tcl82.so.1.0@tcl.so@ |\
	 sed -e s@tk82@tk@ | sed s@tcl82@tcl@ | sed s@wish82.@wish@g
