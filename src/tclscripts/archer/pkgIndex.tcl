# Archer and ArcherCore load Tk-dependent packages.  Register them explicitly
# so generating this index does not require a display or depend on the Itk
# implementation found by the build-time Tcl interpreter.

package ifneeded Archer 1.0 [list source [file join $dir Archer.tcl]]
package ifneeded ArcherCore 1.0 [list source [file join $dir ArcherCore.tcl]]

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
