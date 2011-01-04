set in [open [lindex $argv 0] r]
set out [open [lindex $argv 1] w]
while {[gets $in line] != -1} {
	switch -regexp -- $line "^$" - {^#} continue
	regsub -all {\\} $line {\\\\} line
	regsub -all {"} $line {\"} line
	puts $out "\"$line\\n\""
}
