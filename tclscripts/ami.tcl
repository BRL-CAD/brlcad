#!/bin/sh
# this is a comment \
eval `machinetype.sh -b`
# this is a comment \
if test -x ${BASEDIR}/bin/itclsh3.0
# this is a comment \
then
# this is a comment \
	ITCLSH_NAME=${BASEDIR}/bin/itclsh3.0
# this is a comment \
elif test -x /usr/local/bin/itclsh3.0 
# this is a comment \
then
# this is a comment \
	ITCLSH_NAME=/usr/local/bin/itclsh3.0
# this is a comment \
elif test -x /usr/bin/itclsh3.0 
# this is a comment \
then
# this is a comment \
	ITCLSH_NAME=/usr/bin/itclsh3.0
# this is a comment \
else
# this is a comment \
	ITCLSH_NAME=itclsh
# this is a comment \
fi
# this is a comment \
echo "ITCLSH_NAME=$ITCLSH_NAME"
# the next line restarts using itclsh \
exec $ITCLSH_NAME "$0" "$@"

foreach arg $argv {
    catch { auto_mkindex $arg *.tcl }
}
