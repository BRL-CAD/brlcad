#!/usr/bin/env bash
#                       F O O T E R . S H
# BRL-CAD
#
# Copyright (c) 2004-2014 United States Government as represented by
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
# This script ensures that a file has an emacs/vi variable footer with
# the requested indentation settings.
#
# The script assumes one file as the argument, so example use might be:
#   find . -type f -and \( -name \*.cxx -or -or -name \*.h \) -not -regex '.*src/other.*' -exec sh/footer.sh {} \;
#
# bash arrays are actually used for convenience, hence why bash and
# not sh.
#
###

FILE="$1"

# where are the tap stops?
tab_width=8

# what level indentation?
indentation=4

# force locale setting to C so things like date output as expected
LC_ALL=C

##################
# validate input #
##################
if [ "x$FILE" = "x" ] ; then
    echo "ERROR: must give the name/path of a file to check/update"
    exit 1
elif [ ! -f "$FILE" ] ; then
    echo "ERROR: unable to find $FILE"
    exit 1
elif [ ! -r "$FILE" ] ; then
    echo "ERROR: unable to read $FILE"
    exit 2
elif [ ! -w "$FILE" ] ; then
    echo "ERROR: unable to write to $FILE"
    exit 2
fi

if [ "x$2" != "x" ] ; then
    echo "ERROR: script only supports single file arguments"
    exit 3
fi


########################
# figure out file type #
########################
# mode is the emacs major mode
# mode_vars are the indentation variables that need to be set
# wrap is whether or not in needs to be incased in /* */
# commentchar is the comment character to prefix each line
##
mode=""
mode_vars=""
wrap=0
commentchar=""

commentstart="/"
commentend="/"

case $FILE in
    *.sh )
	echo "$FILE is a shell script"
	mode="sh"
	mode_vars="sh-indentation sh-basic-offset"
	wrap=0
	commentchar="#"
	;;
    *.c )
	echo "$FILE is a C source file"
	mode="C"
	mode_vars=""
	wrap=1
	commentchar="*"
	;;
    *.h )
	echo "$FILE is a C header"
	mode="C"
	mode_vars=""
	wrap=1
	commentchar="*"
	;;
    *.cc | *.cp | *.cxx | *.cpp | *.CPP | *.c++ | *.C )
	echo "$FILE is a C++ source file"
	mode="C++"
	mode_vars="c-basic-offset"
	wrap=0
	commentchar="//"
	;;
    *.hh | *.hp | *.hxx | *.hpp | *.HPP | *.h++ | *.H )
	echo "$FILE is a C++ header"
	mode="C++"
	mode_vars="c-basic-offset"
	wrap=0
	commentchar="//"
	;;
    *.java )
	echo "$FILE is a Java source file"
	mode="Java"
	mode_vars="c-basic-offset"
	wrap=0
	commentchar="//"
	;;
    *.m )
	echo "$FILE is an Objective C-source file"
	mode="objc"
	mode_vars="c-basic-offset"
	wrap=1
	commentchar="*"
	;;
    *.mm | *.M )
	echo "$FILE is an Objective-C++ source file"
	mode="objc"
	mode_vars="c-basic-offset"
	wrap=1
	commentchar="*"
	;;
    *.l )
	echo "$FILE is a Lex/Flex lexer source file"
	mode="C"
	mode_vars=""
	wrap=1
	commentchar="*"
	;;
    *.y )
	echo "$FILE is a Yacc parser source file"
	mode="C"
	mode_vars=""
	wrap=1
	commentchar="*"
	;;
    *.tcl )
	echo "$FILE is a Tcl source file"
	mode="Tcl"
	mode_vars="c-basic-offset tcl-indent-level"
	wrap=0
	commentchar="#"
	;;
    *.tk )
	echo "$FILE is a Tk source file"
	mode="Tcl"
	mode_vars="c-basic-offset tcl-indent-level"
	wrap=0
	commentchar="#"
	;;
    *.itcl )
	echo "$FILE is a IncrTcl source file"
	mode="Tcl"
	mode_vars="c-basic-offset tcl-indent-level"
	wrap=0
	commentchar="#"
	;;
    *.itk )
	echo "$FILE is a IncrTk source file"
	mode="Tcl"
	mode_vars="c-basic-offset tcl-indent-level"
	wrap=0
	commentchar="#"
	;;
    *.php* )
	echo "$FILE is a PHP source file"
	mode="php"
	mode_vars="c-basic-offset"
	wrap=1
	commentchar=""
	commentstart="<!--"
	commentend="-->"
	;;
    *.htm* )
	echo "$FILE is an HTML source file"
	mode="html"
	mode_vars="sgml-basic-offset"
	wrap=1
	commentchar=""
	commentstart="<!--"
	commentend="-->"
	;;
    *.pl )
	echo "$FILE is a Perl source file"
	mode="Perl"
	mode_vars="c-basic-offset perl-indent-level"
	wrap=0
	commentchar="#"
	;;
    *.py )
	echo "$FILE is a Python source file"
	mode="Python"
	mode_vars="c-basic-offset python-indent-level"
	wrap=0
	commentchar="#"
	;;
    *.m4 )
	echo "$FILE is an M4 source file"
	mode="m4"
	mode_vars="standard-indent"
	wrap=0
	commentchar="#"
	;;
    *.am )
	echo "$FILE is an Automake template file"
	mode="Makefile"
	wrap=0
	commentchar="#"
	# override any indent since 8 is required
	indentation=8
	tab_width=8
	;;
    *.ac )
	echo "$FILE is an Autoconf template file"
	mode="autoconf"
	wrap=0
	commentchar="#"
	;;
    *.cmake.in )
	echo "$FILE is a CMake template file"
	mode="cmake"
	wrap=0
	commentchar="#"
	indentation=2
	;;
    *.in )
	echo "$FILE is an Autoconf template file"
	mode="autoconf"
	wrap=0
	commentchar="#"
	;;
    *.m4 )
	echo "$FILE is an m4 macro file"
	mode="m4"
	wrap=0
	commentchar="#"
	# override any indent since 8 is required
	indentation=8
	tab_width=8
	;;
    *.mk )
	echo "$FILE is a make file"
	mode="Makefile"
	wrap=0
	commentchar="#"
	# override any indent since 8 is required
	indentation=8
	tab_width=8
	;;
    *.bat )
	echo "$FILE is a batch shell script"
	mode="sh"
	mode_vars="sh-indentation sh-basic-offset"
	wrap=0
	commentchar="REM"
	;;
    *.vim )
	echo "$FILE is a VIM syntax file"
	mode="tcl"
	mode_vars="c-basic-offset tcl-indent-level"
	wrap=0
	commentchar="\""
	;;
    *.el )
	echo "$FILE is an Emacs Lisp file"
	mode="Lisp"
	mode_vars="lisp-indent-offset"
	wrap=0
	commentchar=";;"
	;;
    *.cmake )
	echo "$FILE is a CMake build file"
	mode="cmake"
	wrap=0
	commentchar="#"
	indentation=2
	;;
    *CMakeLists.txt )
	echo "$FILE is a CMake build file"
	mode="cmake"
	wrap=0
	commentchar="#"
	indentation=2
	;;
    *.[0-9] )
	echo "$FILE is a manual page"
	mode="nroff"
	wrap=0
	commentchar=".\\\""
	# override any indent since 8 is required
	indentation=8
	tab_width=8
	;;
    * )
	# check the first line, see if it is a script
	filesig="`head -n 1 $FILE`"
	case $filesig in
	    */bin/sh )
		echo "$FILE is a shell script"
		mode="sh"
		mode_vars="sh-indentation sh-basic-offset"
		wrap=0
		commentchar="#"
		;;
	    */bin/tclsh )
		echo "$FILE is a Tcl script"
		mode="Tcl"
		mode_vars="c-basic-offset tcl-indent-level"
		wrap=0
		commentchar="#"
		;;
	    */bin/wish )
		echo "$FILE is a Tk script"
		mode="Tcl"
		mode_vars="c-basic-offset tcl-indent-level"
		wrap=0
		commentchar="#"
		;;
	    */bin/perl )
		echo "$FILE is a Perl script"
		mode="Perl"
		mode_vars="c-basic-offset perl-indent-level"
		wrap=0
		commentchar="#"
		;;
	    */bin/python )
		echo "$FILE is a Python script"
		mode="Python"
		mode_vars="c-basic-offset python-indent-level"
		wrap=0
		commentchar="#"
		;;
	    * )
		echo "ERROR: $FILE has an unknown filetype"
		exit 4
		;;
	esac
esac


#################################
# prepare emacs variable arrays #
#################################
variables=( mode ${variables[@]} )
values=( $mode ${values[@]} )
variables=( tab-width ${variables[@]} )
values=( $tab_width ${values[@]} )
for mv in $mode_vars ; do
    variables=( ${variables[@]} $mv )
    values=( ${values[@]} $indentation )
done
variables=( ${variables[@]} indent-tabs-mode )
values=( ${values[@]} t )
if [ "x$mode" = "xC" -o "x$mode" = "xC++" ] ; then
    variables=( ${variables[@]} c-file-style )
    values=( ${values[@]} "\"stroustrup\"" )
fi


##############################
# generate the comment block #
##############################
comment_block="
"
if [ "x$wrap" = "x1" ] ; then
    comment_block="${comment_block}${commentstart}${commentchar}
"
fi

if [ "x$wrap" = "x1" ] ; then
    # pretty-indent the comment block
    prefixspace=" "
fi

# use temp vars so that emacs doesn't think this line is a local var block too
do_not="Local"
parse="Variables"
comment_block="${comment_block}${prefixspace}${commentchar} ${do_not} ${parse}:
"

index=0
for var in ${variables[@]} ; do
    comment_block="${comment_block}${prefixspace}${commentchar} ${var}: ${values[$index]}
"
    index="`expr $index \+ 1`"
done

comment_block="${comment_block}${prefixspace}${commentchar} End:
"
comment_block="${comment_block}${prefixspace}${commentchar} ex: shiftwidth=$indentation tabstop=$tab_width"

if [ "x$wrap" = "x1" ] ; then
    comment_block="${comment_block}
"
    comment_block="${comment_block} ${commentchar}${commentend}"
fi


#########################
# check the emacs block #
#########################
matching_found=0
index=0
for var in ${variables[@]} ; do
    if [ ! "x$commentchar" = "x" ] ; then
	existing_var=`grep -i "${prefixspace}[${commentchar}][${commentchar}]* ${var}:" "$FILE" | sed 's/:/ /g' | awk '{print $3}'`
	existing_suffix=`grep -i "${prefixspace}[${commentchar}][${commentchar}]* ${var}:" "$FILE" | sed 's/:/ /g' | awk '{print $4}'`
    fi

    if [ "x$existing_var" = "x" ] ; then
	echo "$var not found"
    else
	matching_found="`expr $matching_found \+ 1`"

#    echo "found $var=$existing_var"
	if [ "x$existing_var" != "x${values[$index]}" ] ; then
	    echo "$var does not match ... fixing"
	    perl -pi -e "s,(${prefixspace}[${commentchar}]+ ${var}:.*),${prefixspace}${commentchar} ${var}: ${values[$index]},i" $FILE
	elif [ "x$existing_suffix" != "x" ] ; then
	    echo "$var has trailing goo ... fixing"
	    perl -pi -e "s,(${prefixspace}[${commentchar}]+ ${var}:.*),${prefixspace}${commentchar} ${var}: ${values[$index]},i" $FILE
	fi

    fi
    index="`expr $index \+ 1`"
done


######################
# check the vi block #
######################
existing_vi="`grep -i "${prefixspace}${commentchar} ex:" "$FILE"`"
if [ "x$existing_vi" = "x" ] ; then
    echo "No vi line found..."
else
    vi_line="${prefixspace}${commentchar} ex: shiftwidth=$indentation tabstop=$tab_width"
    if [ "x$existing_vi" != "x$vi_line" ] ; then
	echo "vi line is wrong ... fixing"
	perl -pi -e "s,(${prefixspace}[${commentchar}]+ ex:.*),${prefixspace}${commentchar} ex: shiftwidth=${indentation} tabstop=${tab_width},i" $FILE
    fi
fi


###################################
# final sanitization of the block #
###################################
if [ $matching_found -eq 0 ] ; then
    # make sure there are no local vars
    do_not="Local"
    match="Variables"
    local=`grep -i "${do_not} ${match}:" "$FILE" | awk '{print $1}'`
    # w00t, no local vars so just dump a shiny new block at the end of the file
    if [ "x$local" = "x" ] ; then
	cat >> $FILE <<EOF
$comment_block
EOF
    else
	echo "Darn.. existing local var block somewhere with no matches (${FILE})"
    fi
elif [ $matching_found -ne $index ] ; then
    echo "Darn.. partial match on needs to be resolved manually (${FILE})"
else
    # make sure open and closing do not have trailing comment chars too
    do_not="Local"
    match="Variables"

    if [ ! "x$commentchar" = "x" ] ; then
	existing_suffix=`grep -i "${prefixspace}[${commentchar}][${commentchar}]* ${do_not} ${match}:" "$FILE" | sed 's/:/ /g' | awk '{print $4}'`
    fi
    if [ "x$existing_suffix" != "x" ] ; then
	echo "loc. var has trailing goo ... fixing"
	perl -pi -e "s,(${prefixspace}[${commentchar}]+ ${do_not} ${match}:.*),${prefixspace}${commentchar} ${do_not} ${match}:,i" $FILE
    fi
    if [ ! "x$commentchar" = "x" ] ; then
	existing_suffix=`grep -i "${prefixspace}[${commentchar}][${commentchar}]* End:" "$FILE" | sed 's/:/ /g' | awk '{print $3}'`
    fi
    if [ "x$existing_suffix" != "x" ] ; then
	echo "end has trailing goo ... fixing"
	perl -pi -e "s,(${prefixspace}[${commentchar}]+ End:.*),${prefixspace}${commentchar} End:,i" $FILE
    fi
fi


# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
