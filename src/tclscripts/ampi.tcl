#!/bin/sh
# This is a comment \
/bin/echo "This is not a shell script"
# This is a comment \
exit

foreach arg $argv {
    catch {pkg_mkIndex $arg *.tcl *.itcl *.itk *.sh}
    puts $arg
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
