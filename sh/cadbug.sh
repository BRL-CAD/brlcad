#!/bin/sh
#			C A D B U G . S H
#
#  A user-friendly shell script to ease the reporting of BRL-CAD bugs.
#
#  Lee Butler, ARL, Sept 1994
#
# $Revision$

if [ -n "$EDITOR" ] ; then
	REPT_EDITOR=$EDITOR
else
	REPT_EDITOR=vi
fi

BUG_REPORT=/tmp/cad_bug.$$

cat > $BUG_REPORT << ___EOF___
Abstract: [Program/library, 40 character or so descriptive subject (for ref)]
Environment: [BRL-CAD Release, Hardware, OS version]
Description:
        [Detailed description of the problem/suggestion/complaint.]

Repeat-By:
        [Describe the sequence of events that causes the problem to occur.]

Fix:
        [Description of how to fix the problem.  If you don't know a
        fix for the problem, don't include this section.]

Please remove the portions in [], and replace with your own remarks.
___EOF___

ACTION=E
while [ 1 ] ; do
	case $ACTION in
		S*|s*)	break ;;
		F*|f*)	exit ;;
		E*|e*)	$REPT_EDITOR $BUG_REPORT ;;
	esac
	
	ACTION=S
	echo "S)end F)orget E)dit : "
	read ACTION
done
	

if [ -z "$USER" ] ; then
	USER=`whoami`
fi


if [ -x /usr/ucb/mail ] ; then
	/usr/ucb/mail -s "BUG REPORT" cad-bugs@arl.mil "$USER" < $BUG_REPORT
	rm -f $BUG_REPORT
	exit
fi

if [ -x /bin/mail ] ; then
	/bin/mail cad-bugs@arl.mil "$USER" < $BUG_REPORT
	rm -f $BUG_REPORT
	exit
fi

/bin/echo "Mail agent not found.  Send file $BUG_REPORT to cad-bugs@arl.mil"
