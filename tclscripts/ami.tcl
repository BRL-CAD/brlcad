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
if test -x ${BRLCAD_ROOT}/bin/btclsh
# this is a comment \
then
# this is a comment \
	BTCLSH=${BRLCAD_ROOT}/bin/btclsh
# this is a comment \
elif test -x /usr/bin/btclsh
# this is a comment \
then
# this is a comment \
	BTCLSH=/usr/bin/btclsh
# this is a comment \
elif test -x /usr/local/bin/btclsh
# this is a comment \
then
# this is a comment \
	BTCLSH=/usr/local/bin/btclsh
# this is a comment \
else
# this is a comment \
	exit 1
# this is a comment \
fi
# the next line restarts using btclsh \
exec $BTCLSH "$0" "$@"

foreach arg $argv {
    catch {auto_mkindex $arg *.tcl *.itcl}
}
