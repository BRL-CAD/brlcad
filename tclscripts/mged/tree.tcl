## -                            T R E E . T C L
# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Copyright Notice -
#       This software is Copyright (C) 1998 by the United States Army
#       in all countries except the USA.  All rights reserved.
#
# Description -
#	Utility routines specific to trees.

## - print_tree
#
# Print the results of tree to the specified file.
# If file exists it will be appended to. Otherwise, file
# will be created.
#
proc print_tree {file args} {
    set fid [open $file a+]
    puts $fid [eval _mged_tree $args]
    close $fid
}
