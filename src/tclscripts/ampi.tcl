#!/bin/sh
# This is a comment \
/bin/echo "This is not a shell script"
# This is a comment \
exit

foreach arg $argv {
    catch {pkg_mkIndex $arg *.tcl *.itcl *.itk *.sh}
    puts $arg
}
