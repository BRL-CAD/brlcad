#!/bin/bash
#
# This script ensures that a file has an emacs/vi variable footer with
# the requested indentation settings.
#
# The script assumes one file as the argument, so example use might be:
#   find . -type f -and \( -name \*.sh -or -name \*.c -or -name \*.h -or -name \*.tcl -or -name \*.tk -or -name \*.itcl -or -name \*.itk -or -name \*.pl \) -not -regex '.*src/other.*' -exec sh/footer.sh {} \;
#
# bash arrays are actually used for convenience, hence why bash and
# not sh.
#
# Author - 
#   Christopher Sean Morrison 
#
# Source -
#   The U.S. Army Research Laboratory
#   Aberdeen Proving Ground, Maryland 21005-5068  USA
###

FILE="$1"

# where are the tap stops?
tab_width=8

# what level indentation?
indentation=4


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
# commentchar is the comment character to prefex each line
###
case $FILE in 
    *.sh)
	echo "$FILE is a shell script"
	mode="sh"
	mode_vars="sh-indentation sh-basic-offset"
	wrap=0
	commentchar="#"
	;;
    *.c)
	echo "$FILE is a C source file"
	mode="C"
	mode_vars="c-basic-offset"
	wrap=1
	commentchar="*"
	;;
    *.h)
	echo "$FILE is a C header"
	mode="C"
	mode_vars="c-basic-offset"
	wrap=1
	commentchar="*"
	;;
    *.tcl)
	echo "$FILE is a Tcl source file"
	mode="Tcl"
	mode_vars="c-basic-offset tcl-indent-level"
	wrap=0
	commentchar="#"
	;;
    *.tk)
	echo "$FILE is a Tk source file"
	mode="Tcl"
	mode_vars="c-basic-offset tcl-indent-level"
	wrap=0
	commentchar="#"
	;;
    *.itcl)
	echo "$FILE is a IncrTcl source file"
	mode="Tcl"
	mode_vars="c-basic-offset tcl-indent-level"
	wrap=0
	commentchar="#"
	;;
    *.itk)
	echo "$FILE is a IncrTk source file"
	mode="Tcl"
	mode_vars="c-basic-offset tcl-indent-level"
	wrap=0
	commentchar="#"
	;;
    *.pl)
	echo "$FILE is a Perl source file"
	mode="Perl"
	mode_vars="c-basic-offset perl-indent-level"
	wrap=0
	commentchar="#"
	;;
    *)
	echo "WARNING: $FILE has an unknown filetype"
	;;
esac


#################################
# prepare emacs variable arrays #
#################################
variables=( ${variables[@]} mode )
values=( ${values[@]} $mode )
variables=( ${variables[@]} tab-width )
values=( ${values[@]} $tab_width )
for mv in $mode_vars ; do
    variables=( ${variables[@]} $mv )
    values=( ${values[@]} $indentation )
done
variables=( ${variables[@]} indent-tabs-mode )
values=( ${values[@]} t )


##############################
# generate the comment block #
##############################
comment_block="
"
if [ "x$wrap" = "x1" ] ; then
    comment_block="${comment_block}/`echo "${commentchar}"`
"
fi

if [ "x$wrap" = "x1" ] ; then
    # pretty-indent the comment block
    prefixspace=" "
fi

# use temp vars so that emacs doesn't think this line is a local var block too
do_not="Local"
parse="Variables"
comment_block="${comment_block}`echo "${prefixspace}${commentchar} ${do_not} ${parse}:"`
"

index=0
for var in ${variables[@]} ; do
    comment_block="${comment_block}`echo "${prefixspace}${commentchar} ${var}: ${values[$index]}"`
"
    index="`expr $index \+ 1`"
done

comment_block="${comment_block}`echo "${prefixspace}${commentchar} End:"`
"
comment_block="${comment_block}`echo "${prefixspace}${commentchar} ex: shiftwidth=$indentation tabstop=$tab_width"`"

if [ "x$wrap" = "x1" ] ; then
    comment_block="${comment_block}
"
    comment_block="${comment_block}`echo " ${commentchar}/"`"
fi


#########################
# check the emacs block #
#########################
matching_found=0
index=0
for var in ${variables[@]} ; do
    existing_var=`cat $FILE | grep -i "${prefixspace}[${commentchar}] ${var}:" | sed 's/:/ /g' | awk '{print $3}'`
    existing_suffix=`cat $FILE | grep -i "${prefixspace}[${commentchar}] ${var}:" | sed 's/:/ /g' | awk '{print $4}'`
    
    if [ "x$existing_var" = "x" ] ; then
	echo "$var not found"
    else
	matching_found="`expr $matching_found \+ 1`"

#    echo "found $var=$existing_var"
	if [ "x$existing_var" != "x${values[$index]}" ] ; then
	    echo "$var does not match ... fixing"
	    perl -pi -e "s/(${prefixspace}[${commentchar}] ${var}:.*)/${prefixspace}${commentchar} ${var}: ${values[$index]}/i" $FILE
	elif [ "x$existing_suffix" != "x" ] ; then
	    echo "$var has trailing goo ... fixing"
	    perl -pi -e "s/(${prefixspace}[${commentchar}] ${var}:.*)/${prefixspace}${commentchar} ${var}: ${values[$index]}/i" $FILE
	fi

    fi
    index="`expr $index \+ 1`"
done


######################
# check the vi block #
######################
existing_vi="`cat $FILE | grep -i "${prefixspace}${commentchar} ex:"`"
if [ "x$existing_vi" = "x" ] ; then
    echo "No vi line found..."
elif [ "x$existing_vi" != "x${prefixspace}${commentchar} ex: shiftwidth=$indentation tabstop=$tab_width" ] ; then
    echo "vi line is wrong ... fixing"
    perl -pi -e "s/(${prefixspace}[${commentchar}] ex:.*)/${prefixspace}${commentchar} ex: shiftwidth=${indentation} tabstop=${tab_width}/i" $FILE
fi


###################################
# final sanitization of the block #
###################################
if [ $matching_found -eq 0 ] ; then
    # make sure there are no local vars
    do_not="Local"
    match="Variables"
    local=`cat "$FILE" | grep -i "${do_not} ${match}:" | awk '{print $1}'`
    # w00t, no local vars so just dump a shiney new block at the end of the file
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
    existing_suffix=`cat $FILE | grep -i "${prefixspace}[${commentchar}] ${do_not} ${match}:" | sed 's/:/ /g' | awk '{print $4}'`
    if [ "x$existing_suffix" != "x" ] ; then
	echo "loc. var has trailing goo ... fixing"
	perl -pi -e "s/(${prefixspace}[${commentchar}] ${do_not} ${match}:.*)/${prefixspace}${commentchar} ${do_not} ${match}:/i" $FILE
    fi
    existing_suffix=`cat $FILE | grep -i "${prefixspace}[${commentchar}] End:" | sed 's/:/ /g' | awk '{print $3}'`
    if [ "x$existing_suffix" != "x" ] ; then
	echo "end has trailing goo ... fixing"
	perl -pi -e "s/(${prefixspace}[${commentchar}] End:.*)/${prefixspace}${commentchar} End:/i" $FILE
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
