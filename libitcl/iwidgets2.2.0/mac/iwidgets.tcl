#
# iwidgets.tcl
# ----------------------------------------------------------------------
# Invoked automatically by [incr Tk] upon startup to initialize
# the [incr Widgets] package.
# ----------------------------------------------------------------------
#  AUTHOR: Mark L. Ulferts               EMAIL: mulferts@spd.dsccc.com
#
#  @(#) $Id$
# ----------------------------------------------------------------------
#                Copyright (c) 1995  Mark L. Ulferts
# ======================================================================
# Permission is hereby granted, without written agreement and without
# license or royalty fees, to use, copy, modify, and distribute this
# software and its documentation for any purpose, provided that the
# above copyright notice and the following two paragraphs appear in
# all copies of this software.
#
# IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
# DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
# ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
# IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.
#
# THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
# BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
# FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
# ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
# PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
# ======================================================================
package require Tcl 7.6
package require Tk 4.2
package require Itcl 2.2
package require Itk 2.2

package provide Iwidgets 2.2.0

#
# Create the namespace that will contain all [incr Widgets]
#
namespace ::iwidgets {}
import add iwidgets

if { $tcl_platform(platform) == "macintosh" } {
	source -rsrc "iwidgets:tclIndex"
}

namespace ::iwidgets {
    public variable version [set ::itk::version].0
    public variable library

    proc _init {} {
        global env tcl_platform library version
        rename _init {}

        #
        # See if the user has set a value for IWIDGETS_LIBRARY.
        # If not, use the value set when this package was installed.
        #
        set lib iwidgets$version
        set dirs ""
        if [info exists env(IWIDGETS_LIBRARY)] {
            lappend dirs $env(IWIDGETS_LIBRARY)
        }
        if [info exists env(EXT_FOLDER)] {
            lappend dirs [file join $env(EXT_FOLDER) {Tool Command Language} itcl2.2 $lib]
        }
        lappend dirs [file join [file dirname [info library]] $lib]
        set parentDir [file dirname [file dirname [info nameofexecutable]]]
        lappend dirs [file join $parentDir lib $lib]
        lappend dirs [file join [file dirname $parentDir] $lib library]
        lappend dirs [file join [file dirname $parentDir] itcl library]
        lappend dirs [file join $parentDir library]

        foreach i $dirs {
            set library $i
            if {[file exists $library]} {
                lappend auto_path $library
                return
            }
        }
        set library {}
        # it is not an error not to find the library on the
        # Mac if it is in the resource fork...
        if { !( $tcl_platform(platform) == "macintosh" && 
        		[lsearch [resource list TEXT] iwidgets] > -1 ) } {
	        set msg "Can't find library $lib\n"
    	    append msg "Perhaps you need to install \[incr Widgets\]\n"
        	append msg "or set your IWIDGETS_LIBRARY environment variable?"
        	error $msg
        }
        
    }
    _init
    lappend auto_path $library
}

