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
# this is a comment \
DOLLAR_ZERO="$0" ; DOLLAR_AT="$@" ; EXEC_COMMAND="$BTCLSH $DOLLAR_ZERO $DOLLAR_AT"
# this is a comment \
echo "Running \`exec $EXEC_COMMAND\`"
# the next line restarts using btclsh \
exec $EXEC_COMMAND

# exec $BTCLSH "$0" "$@"

foreach arg $argv {
    catch {auto_mkindex $arg *.tcl *.itcl}
}
