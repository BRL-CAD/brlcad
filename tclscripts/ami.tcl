#!/bin/sh
# the next line restarts using tclsh \
exec tclsh "$0" "$@"

foreach arg $argv {
    puts "auto_mkindex $arg *.tcl"
    catch { auto_mkindex $arg *.tcl }
}
