#!/bin/sh
# the next line restarts using tclsh \
exec tclsh8.0 "$0" "$@"

foreach arg $argv {
    catch { auto_mkindex $arg *.tcl }
}
