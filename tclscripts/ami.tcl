#!/bin/sh
# this is a comment \
if test "$BRLCAD_ROOT" = ""
# this is a comment \
then
# this is a comment \
	BRLCAD_ROOT=/usr/brlcad
# this is a comment \
	export BRLCAD_ROOT
# this is a comment \
fi

# this is a comment \
if test -x ${BRLCAD_ROOT}/bin/bwish
# this is a comment \
then
# this is a comment \
	TCL_PROG=${BRLCAD_ROOT}/bin/bwish
# this is a comment \
else
# this is a comment \
	exit 1
# this is a comment \
fi
# the next line restarts using bwish \
exec $TCL_PROG "$0" "$@"

# Use the following line with bwish
wm withdraw .

# Use the following two lines with tclsh
#load $env(BRLCAD_ROOT)/lib/libitcl.so
#auto_mkindex_parser::slavehook {_%@namespace import -force ::itcl::*}

foreach arg $argv {
    catch { auto_mkindex $arg *.tcl }
}

# Use the following line with bwish
exit
