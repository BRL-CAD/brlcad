#!/bin/sh

# svn2cl.sh - front end shell script for svn2cl.xsl, calls xsltproc
#             with the correct parameters
#
# Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2013 Arthur de Jong
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
# 3. The name of the author may not be used to endorse or promote
#    products derived from this software without specific prior
#    written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# exit on any failures
set -e
# report unset variables
set -u

# svn2cl version
VERSION="0.14"

# set default parameters
PWD=`pwd`
STRIPPREFIX="AUTOMATICALLY-DETERMINED"
LINELEN=75
GROUPBYDAY="no"
INCLUDEREV="no"
BREAKBEFOREMSG="no"
REPARAGRAPH="no"
SEPARATEDAYLOGS="no"
ACTIONS="no"
CHANGELOG=""
OUTSTYLE="cl"
SVNLOGCMD="svn --verbose --xml log"
SVNINFOCMD="svn --non-interactive info"
AUTHORSFILE=""
IGNORE_MESSAGE_STARTING=""
TITLE="ChangeLog"
REVISION_LINK="#r"
TICKET_LINK=""
TICKET_PREFIX="#"
TMPFILES=""
AWK="awk"

# do command line checking
prog=`basename "$0"`
while [ $# -gt 0 ]
do
  case "$1" in
    --strip-prefix)
      STRIPPREFIX="$2"
      shift 2 || { echo "$prog: option requires an argument -- $1";exit 1; }
      ;;
    --strip-prefix=*)
      STRIPPREFIX=`echo "$1" | sed 's/^--[a-z-]*=//'`
      shift
      ;;
    --linelen)
      LINELEN="$2";
      shift 2 || { echo "$prog: option requires an argument -- $1";exit 1; }
      ;;
    --linelen=*)
      LINELEN=`echo "$1" | sed 's/^--[a-z-]*=//'`
      shift
      ;;
    --group-by-day)
      GROUPBYDAY="yes";
      shift
      ;;
    --separate-daylogs)
      SEPARATEDAYLOGS="yes"
      shift
      ;;
    -i|--include-rev)
      INCLUDEREV="yes";
      shift
      ;;
    -a|--include-actions)
      ACTIONS="yes"
      shift
      ;;
    --break-before-msg|--breaks-before-msg)
      # FIXME: if next argument is numeric use that as a parameter
      BREAKBEFOREMSG="yes"
      shift
      ;;
    --break-before-msg=*|--breaks-before-msg=*)
      BREAKBEFOREMSG=`echo "$1" | sed 's/^--[a-z-]*=//'`
      shift
      ;;
    --reparagraph)
      REPARAGRAPH="yes"
      shift
      ;;
    --title)
      TITLE="$2"
      shift 2 || { echo "$prog: option requires an argument -- $1";exit 1; }
      ;;
    --title=*)
      TITLE=`echo "$1" | sed 's/^--[a-z-]*=//'`
      shift
      ;;
    --revision-link)
      REVISION_LINK="$2"
      shift 2 || { echo "$prog: option requires an argument -- $1";exit 1; }
      ;;
    --revision-link=*)
      REVISION_LINK=`echo "$1" | sed 's/^--[a-z-]*=//'`
      shift
      ;;
    --ticket-link)
      TICKET_LINK="$2"
      shift 2 || { echo "$prog: option requires an argument -- $1";exit 1; }
      ;;
    --ticket-link=*)
      TICKET_LINK=`echo "$1" | sed 's/^--[a-z-]*=//'`
      shift
      ;;
    --ticket-prefix)
      TICKET_PREFIX="$2"
      shift 2 || { echo "$prog: option requires an argument -- $1";exit 1; }
      ;;
    --ticket-prefix=*)
      TICKET_PREFIX=`echo "$1" | sed 's/^--[a-z-]*=//'`
      shift
      ;;
    --ignore-message-starting)
      IGNORE_MESSAGE_STARTING="$2"
      shift 2 || { echo "$prog: option requires an argument -- $1";exit 1; }
      ;;
    --ignore-message-starting=*)
      IGNORE_MESSAGE_STARTING=`echo "$1" | sed 's/^--[a-z-]*=//'`
      shift
      ;;
    -f|--file|-o|--output)
      CHANGELOG="$2"
      shift 2 || { echo "$prog: option requires an argument -- $1";exit 1; }
      ;;
    --file=*|--output=*)
      CHANGELOG=`echo "$1" | sed 's/^--[a-z-]*=//'`
      shift
      ;;
    --stdout)
      CHANGELOG="-"
      shift
      ;;
    --authors)
      AUTHORSFILE="$2"
      shift 2 || { echo "$prog: option requires an argument -- $1";exit 1; }
      ;;
    --authors=*)
      AUTHORSFILE=`echo "$1" | sed 's/^--[a-z-]*=//'`
      shift
      ;;
    --html)
      OUTSTYLE="html"
      shift
      ;;
    -r|--revision|-c|--change|--targets|-l|--limit)
      # add these as extra options to the log command (with argument)
      arg=`echo "$2" | sed "s/'/'\"'\"'/g"`
      SVNLOGCMD="$SVNLOGCMD $1 '$arg'"
      shift 2 || { echo "$prog: option requires an argument -- $1";exit 1; }
      ;;
    --revision=*|--change=*|--targets=*|--limit=*)
      # these are single argument versions of the above (with argument)
      arg=`echo "$1" | sed "s/'/'\"'\"'/g"`
      SVNLOGCMD="$SVNLOGCMD '$arg'"
      shift
      ;;
    --username|--password|--config-dir|--config-option)
      # add these as extra options to the log and info commands (with argument)
      arg=`echo "$2" | sed "s/'/'\"'\"'/g"`
      SVNLOGCMD="$SVNLOGCMD $1 '$arg'"
      SVNINFOCMD="$SVNINFOCMD $1 '$arg'"
      shift 2 || { echo "$prog: option requires an argument -- $1";exit 1; }
      ;;
    --username=*|--password=*|--config-dir=*|--config-option=*)
      # these are single argument versions of the above (with argument)
      arg=`echo "$1" | sed "s/'/'\"'\"'/g"`
      SVNLOGCMD="$SVNLOGCMD '$arg'"
      SVNINFOCMD="$SVNINFOCMD '$arg'"
      shift
      ;;
    -g|--use-merge-history|--stop-on-copy)
      # add these as simple options to the log command
      SVNLOGCMD="$SVNLOGCMD $1"
      shift
      ;;
    --no-auth-cache|--non-interactive|--trust-server-cert)
      # add these as simple options to both the log and info commands
      SVNLOGCMD="$SVNLOGCMD $1"
      SVNINFOCMD="$SVNINFOCMD $1"
      shift
      ;;
    -V|--version)
      echo "$prog $VERSION";
      echo "Written by Arthur de Jong."
      echo ""
      echo "Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2013 Arthur de Jong."
      echo "This is free software; see the source for copying conditions.  There is NO"
      echo "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE."
      exit 0
      ;;
    -h|--help)
      echo "Usage: $prog [OPTION]... [PATH]..."
      echo "Generate a ChangeLog from a subversion repository."
      echo ""
      echo "  --strip-prefix=NAME  prefix to strip from all entries, defaults path"
      echo "                       inside the repository"
      echo "  --linelen=NUM        maximum length of an output line"
      echo "  --group-by-day       group changelog entries by day"
      echo "  --separate-daylogs   put a blank line between grouped by day entries"
      echo "  -i, --include-rev    include revision numbers"
      echo "  -a, --include-actions     add [ADD], [DEL] and [CPY] tags to files"
      echo "  --break-before-msg[=NUM]  add a line break (or multiple breaks)"
      echo "                       between the paths and the log message"
      echo "  --reparagraph        rewrap lines inside a paragraph"
      echo "  --title=NAME         title used in html file"
      echo "  --revision-link=NAME link revision numbers in html output"
      echo "  --ticket-link=NAME   change #foo strings to links"
      echo "  --ignore-message-starting=STRING"
      echo "                       ignore messages starting with the string"
      echo "  -o, --output=FILE    output to FILE instead of ChangeLog"
      echo "  -f, --file=FILE      alias for -o, --output"
      echo "  --stdout             output to stdout instead of ChangeLog"
      echo "  --authors=FILE       file to read for authors"
      echo "  --html               output as html instead of plain text"
      echo "  -h, --help           display this help and exit"
      echo "  -V, --version        output version information and exit"
      echo ""
      echo "PATH arguments and the following options are passed to the svn log"
      echo "command: -r, --revision, -g, --use-merge-history, -c, --change,"
      echo "--targets, --stop-on-copy, -l, --username, --password, --no-auth-cache,"
      echo "--non-interactive, --trust-server-cert, --config-dir and --config-option"
      echo "(see 'svn help log' for more information)."
      exit 0
      ;;
    -*)
      echo "$prog: invalid option -- $1"
      echo "Try '$prog --help' for more information."
      exit 1
      ;;
    *)
      arg=`echo "$1" | sed "s/'/'\"'\"'/g"`
      SVNLOGCMD="$SVNLOGCMD '$arg'"
      SVNINFOCMD="$SVNINFOCMD '$arg'"
      shift
      ;;
  esac
done

# find the directory that this script resides in
prog="$0"
while [ -h "$prog" ]
do
  dir=`dirname "$prog"`
  prog=`ls -ld "$prog" | sed "s/^.*-> \(.*\)/\1/;/^[^/]/s,^,$dir/,"`
done
dir=`dirname "$prog"`
dir=`cd "$dir" && pwd`
XSL="$dir/svn2${OUTSTYLE}.xsl"

# check if the authors file is formatted as a legacy
# colon separated file
if [ -n "$AUTHORSFILE" ] && \
    egrep '^(#.*|[a-zA-Z0-9].*:)' "$AUTHORSFILE" > /dev/null 2>/dev/null
then
  # create a temporary file
  tmpfile=`mktemp -t svn2cl.XXXXXX 2> /dev/null || tempfile -s .svn2cl 2> /dev/null || echo "$AUTHORSFILE.$$.xml"`
  arg=`echo "$tmpfile" | sed "s/'/'\"'\"'/g"`
  TMPFILES="$TMPFILES '$arg'"
  # generate an authors.xml file on the fly
  echo '<authors>' > "$tmpfile"
  sed -n 's/&/\&amp;/g;s/</\&lt;/g;s/>/\&gt;/g;s|^\([a-zA-Z0-9][^:]*\):\(.*\)$| <author uid="\1">\2</author>|p' \
      < "$AUTHORSFILE"  >> "$tmpfile"
  echo '</authors>' >> "$tmpfile"
  AUTHORSFILE="$tmpfile"
fi

# find the absolute path of the authors file
# (otherwise xsltproc will find the file relative to svn2cl.xsl)
pwd=`pwd`
AUTHORSFILE=`echo "$AUTHORSFILE" | sed "/^[^/]/s|^|$pwd/|"`

# if no filename was specified, make one up
if [ -z "$CHANGELOG" ]
then
  CHANGELOG="ChangeLog"
  if [ "$OUTSTYLE" != "cl" ]
  then
    CHANGELOG="$CHANGELOG.$OUTSTYLE"
  fi
fi

# try to determin a prefix to strip from all paths
if [ "$STRIPPREFIX" = "AUTOMATICALLY-DETERMINED" ]
then
  STRIPPREFIX=`LANG=C eval "$SVNINFOCMD" | $AWK '/^URL:/{url=$2} /^Repository Root:/{root=$3} END{if(root){print substr(url,length(root)+2)}else{n=split(url,u,"/");print u[n]}}'`
  STRIPPREFIX=`echo "$STRIPPREFIX" | sed 's/%20/ /g'`
fi

# redirect stdout to the changelog file if needed
if [ "x$CHANGELOG" != "x-" ]
then
  exec > "$CHANGELOG"
fi

# actually run the command we need
eval "$SVNLOGCMD" | \
  xsltproc --stringparam strip-prefix "$STRIPPREFIX" \
           --stringparam linelen "$LINELEN" \
           --stringparam groupbyday "$GROUPBYDAY" \
           --stringparam separate-daylogs "$SEPARATEDAYLOGS" \
           --stringparam include-rev "$INCLUDEREV" \
           --stringparam include-actions "$ACTIONS" \
           --stringparam breakbeforemsg "$BREAKBEFOREMSG" \
           --stringparam reparagraph "$REPARAGRAPH" \
           --stringparam authorsfile "$AUTHORSFILE" \
           --stringparam title "$TITLE" \
           --stringparam revision-link "$REVISION_LINK" \
           --stringparam ticket-link "$TICKET_LINK" \
           --stringparam ticket-prefix "$TICKET_PREFIX" \
           --stringparam ignore-message-starting "$IGNORE_MESSAGE_STARTING" \
           --nowrite \
           --nomkdir \
           --nonet \
           "$XSL" -

# clean up temporary files
[ -n "$TMPFILES" ] && eval "rm -f $TMPFILES"

# we're done (the previous command could return false)
exit 0
