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
if test -x ${BRLCAD_ROOT}/bin/tclsh
# this is a comment \
then
# this is a comment \
	TCLSH=${BRLCAD_ROOT}/bin/tclsh
# this is a comment \
elif test -x /usr/bin/tclsh
# this is a comment \
then
# this is a comment \
	TCLSH=/usr/bin/tclsh
# this is a comment \
elif test -x /usr/local/bin/tclsh
# this is a comment \
then
# this is a comment \
	TCLSH=/usr/local/bin/tclsh
# this is a comment \
else
# this is a comment \
	exit 1
# this is a comment \
fi
# the next line restarts using tclsh \
exec $TCLSH "$0" "$@"

package require Itcl
auto_mkindex_parser::slavehook {_%@namespace import -force ::itcl::*}

foreach arg $argv {
    catch {auto_mkindex $arg *.tcl *.itcl}
}
