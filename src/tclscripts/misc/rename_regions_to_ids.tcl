#       R E N A M E _ R E G I O N S _ T O _ I D S . T C L
# BRL-CAD
#
# Published in 2023 by the United States Government.
# This work is in the public domain.
#
###
# This script iterates over all regions, and for regions with a number
# in its name, it sets region_id to that number, and renames the
# object with that number and a .r suffix.
#
# Example: source rename_regions_to_ids.tcl
#
# Tree Before:
#  all
#    some_region_1234 {region_id=1000 }
#      prim1
#    another_comb
#      some_region {region_id=2000}
#        prim2
#
# Tree After:
#  all
#    1234.r {region_id=1234 }
#      prim1
#    another_comb
#      some_region {region_id=2000}
#        prim2

set glob_compat_mode_backup $glob_compat_mode
set glob_compat_mode 0

# returns first number found in a string or empty string otherwise
proc extractNumber {inputString} {
    if {[regexp {(\d+)} $inputString match number]} {
        return $number
    } else {
        return ""
    }
}

# renames and sets id on all regions with a number in the name
foreach region [ search . -type region ] {
    set id [ extractNumber $region ]
    if { $id == "" } continue

    attr set $region region_id $id
    mv $region $id.r
}

set glob_compat_mode $glob_compat_mode_backup


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
