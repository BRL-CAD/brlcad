#
# source all tests.
#
package require tcltest 2.1

package require Tk 8.5 ;# This is the Tk test suite; fail early if no Tk!

tcltest::configure -testdir [file join [pwd] [file dirname [info script]]]

eval tcltest::configure $::argv
tcltest::runAllTests

if {![catch { package present Tk }]} {
	destroy .
}
