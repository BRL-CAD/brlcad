#!/usr/bin/env tclsh
# material_roundtrip.tcl - test material import/export round-trip

set ascfile [file join $srcdir 00_moss-control.asc]
set dbfile material_roundtrip.g
set matfile [file join $srcdir matprop.example]
set expfile exported_materials.example

# Clean start
catch { file delete $dbfile $expfile }

# Convert base geometry
opendb $dbfile create
asc2g $ascfile

# Import materials
material import --t matprop $matfile

# Export all materials
material export $expfile

# Compare with original (allowing minor formatting differences)
set orig [read_file $matfile]
set exported [read_file $expfile]

# Normalize whitespace and empty lines for comparison
regsub -all {[ \t\r\n]+} $orig " " orig_norm
regsub -all {[ \t\r\n]+} $exported " " exported_norm

if {$orig_norm == $exported_norm} {
    puts "PASSED: material import/export round-trip"
    exit 0
} else {
    puts "FAILED: exported materials differ from source"
    exit 1
}