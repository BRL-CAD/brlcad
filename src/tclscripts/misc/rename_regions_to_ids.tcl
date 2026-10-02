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

namespace eval ::brlcad::mged::rename_regions_to_ids {}

# Returns the first number found in a string or an empty string.
proc ::brlcad::mged::rename_regions_to_ids::extract_number {input_string} {
    if {[regexp {(\d+)} $input_string -> number]} {
        return $number
    }

    return ""
}

# Renames and sets the ID on all regions with a number in the name.
proc ::brlcad::mged::rename_regions_to_ids::run {} {
    set old_glob_compat_mode $::glob_compat_mode
    set ::glob_compat_mode 0
    try {
        foreach region [::brlcad::mged search . -type region] {
            set id [extract_number $region]
            if {$id eq ""} {
                continue
            }

            ::brlcad::mged attr set $region region_id $id
            ::brlcad::mged mv $region $id.r
        }
    } finally {
        set ::glob_compat_mode $old_glob_compat_mode
    }
}

::brlcad::mged::rename_regions_to_ids::run


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
