#!/bin/sh

sed -e s@lib/tcl\$\(VERSION\)@tcl\$\(VERSION\)@ Makefile |\
	 sed -e s@\(prefix\)/include@\(prefix\)/include/brlcad@ |\
	 sed -e "s@\${\C\C}  \${TCLSH_OBJS}@\${\C\C} $LDFLAGS \${TCLSH_OBJS}@" |\
	 sed -e "s@\${\C\C}  \${TCLTEST_OBJS}@\${\C\C} $LDFLAGS \${TCLTEST_OBJS}@" |\
         sed -e "s@\$(BIN_INSTALL_DIR)/tclsh\$(VERSION)@\$(BIN_INSTALL_DIR)/tclsh@"
