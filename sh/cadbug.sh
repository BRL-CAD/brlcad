#!/bin/sh
#                       C A D B U G . S H
# BRL-CAD
#
# Copyright (c) 2004-2007 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###
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
		F*|f*)	rm -f $BUG_REPORT ; exit ;;
		E*|e*)	$REPT_EDITOR $BUG_REPORT ;;
	esac

	ACTION=S
	echo "S)end F)orget E)dit : "
	read ACTION
done


if [ -z "$USER" ] ; then
	USER=`whoami`
fi

BUGEMAIL=bugs@brlcad.org

FAILED=0
if [ -x /usr/ucb/mail ] ; then
	/usr/ucb/mail -s "BUG REPORT" $BUGEMAIL "$USER" < $BUG_REPORT
	if [ $? -eq 0 ] ; then
		rm -f $BUG_REPORT
		exit
	else
		FAILED=1
		echo "/usr/ucb/mail exited with non-zero status."
		echo "message file $BUG_REPORT not deleted"
	fi
fi

if [ -x /usr/bsd/Mail ] ; then
	/usr/bsd/Mail -s "BUG REPORT" $BUGEMAIL "$USER" < $BUG_REPORT
	if [ $? -eq 0 ] ; then
		rm -f $BUG_REPORT
		exit
	else
		FAILED=1
		echo "/usr/bsd/Mail exited with non-zero status."
		echo "message file $BUG_REPORT not deleted"
	fi
fi

if [ -x /bin/mail ] ; then
	/bin/mail $BUGEMAIL "$USER" < $BUG_REPORT
	if [ $? -eq 0 ] ; then
		rm -f $BUG_REPORT
	else
		FAILED=1
		echo "/bin/mail exited with non-zero status."
		echo "message file $BUG_REPORT not deleted"
	fi
	exit
fi

if [ -x /usr/bin/mail ] ; then
	/usr/bin/mail $BUGEMAIL "$USER" < $BUG_REPORT
	if [ $? -eq 0 ] ; then
		rm -f $BUG_REPORT
	else
		FAILED=1
		echo "/usr/bin/mail exited with non-zero status."
		echo "message file $BUG_REPORT not deleted"
	fi
	exit
fi

if [ $FAILED -eq 1] ; then
	/bin/echo "Mail delivery failed.  Send file $BUG_REPORT to $BUGEMAIL"
else
	/bin/echo "Mail agent not found.  Send file $BUG_REPORT to $BUGEMAIL"
fi

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
