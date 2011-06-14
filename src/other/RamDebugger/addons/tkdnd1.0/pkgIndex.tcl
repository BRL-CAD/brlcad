namespace eval ::dnd {
   ##
   ## This function will allow the loading the shared library, found in the
   ## following places:
   ## 1) The first location searched is a directory derived from the platform
   ## we are running. For example under windows an attempt will be made to
   ## load the library from the "Windows" subdirectory, or from "Linux", if
   ## we are running under Linux. This allows us to ship tkdnd with support
   ## for multiple operating systems simultaneously.
   ##
   ## 2) If the above fails, we revert to the original behaviour of "load".
   proc _load {dir} {
      set version 1.0
      switch $::tcl_platform(platform) {
         windows {
            if {[catch {load [file join $dir libtkdnd[string map {. {}} \
                $version][info sharedlibextension]] tkdnd} error]} {
               ## The library was not found. Perhaps under a directory with the
               ## OS name?
               if {[catch {load [file join $dir Windows \
                   libtkdnd[string map {. {}} $version][info \
                   sharedlibext]] tkdnd} error2]} {
                  return -code error "$error\n$error2"
               }
            }
         }
         default {
            if {[catch {load [file join $dir \
                libtkdnd$version[info sharedlibextension]] tkdnd} error]} {
               ## The library was not found. Perhaps under a directory with the
               ## OS name?
               if {[catch {load [file join $dir $::tcl_platform(os) \
                            libtkdnd$version[info sharedlibextension]] tkdnd}]} {
                  return -code error $error
               }
            }
         }
      }
      source [file join $dir tkdnd.tcl]
      package provide tkdnd $version
  }
}

package ifneeded tkdnd 1.0  [list ::dnd::_load $dir]
