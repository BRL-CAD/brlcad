#!/usr/bin/env tclsh
# material_3way_conflict.tcl - test material conflict detection in 3-way merge

set left_asc   [file join $srcdir 01_moss-diff.asc]
set anc_asc    [file join $srcdir 00_moss-control.asc]
set right_asc  [file join $srcdir 02_moss-3diff.asc]
set matfile    [file join $srcdir matprop.example]

opendb left.g create;  asc2g $left_asc;  material import --type matprop $matfile
opendb anc.g create;   asc2g $anc_asc
opendb right.g create;  asc2g $right_asc

# Modify Air density only on right branch to create conflict
opendb right.g
material create Air density 0.005

# Run 3-way diff with merge
catch { exec gdiff left.g anc.g right.g --merge merged.g } result

opendb merged.g
material export > exported_materials.example

# We expect to see both versions of Air present (or conflict markers)
set content [read_file exported_materials.example]
if {[string match "*Air*" $content] && [llength [regexp -all -inline {Density.*0\.001} $content]] \
    && [llength [regexp -all -inline {Density.*0\.005} $content]]} {
    puts "PASSED: 3-way material conflict correctly preserved"
    exit 0
} else {
    puts "FAILED: material conflict not preserved"
    exit 1
}