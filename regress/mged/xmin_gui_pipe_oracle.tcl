set tolerance 1.0e-9
set expected_attributes {
    V0 {0 0 0} O0 1 I0 0.5 R0 2
    V1 {4 0 2} O1 1 I1 0.5 R1 2
    V2 {6 2 2} O2 1 I2 0.5 R2 2
    V3 {8 4 4} O3 1 I3 0.5 R3 2
    V4 {12 4 6} O4 1 I4 0.5 R4 2
}

if {![catch {get gui.pipe V5}]} {
    error "pipe split produced more than five control points"
}

foreach {attribute expected} $expected_attributes {
    set actual [get gui.pipe $attribute]
    if {[llength $actual] != [llength $expected]} {
	error "pipe attribute $attribute is {$actual}, expected {$expected}"
    }
    foreach actual_value $actual expected_value $expected {
	if {abs(double($actual_value) - double($expected_value)) > $tolerance} {
	    error "pipe attribute $attribute is {$actual}, expected {$expected}"
	}
    }
}

puts "PASS: pipe split stored the expected five-point geometry"
