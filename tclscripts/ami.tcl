#!/bin/sh
# the next line restarts using tclsh \
exec tclsh "$0" "$@"

foreach arg $argv {
    catch { auto_mkindex $arg *.tcl }
}
