#!/bin/sh
# this is a comment \
eval `machinetype.sh -b`
# this is a comment \
if test -x ${BASEDIR}/bin/tclsh
# this is a comment \
then
# this is a comment \
	TCLSH_NAME=tclsh
# this is a comment \
elif test -x /usr/local/bin/tclsh8.0 
# this is a comment \
then
# this is a comment \
	TCLSH_NAME=tclsh8.0
# this is a comment \
else
# this is a comment \
	TCLSH_NAME=tclsh
# this is a comment \
fi
# the next line restarts using tclsh \
exec $TCLSH_NAME "$0" "$@"

foreach arg $argv {
    catch { auto_mkindex $arg *.tcl }
}
