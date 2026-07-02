// Control flow, user modules with defaults, ranges, hull, and modifiers.
// Exercises: module + default params, if/else, for over [a:step:b],
// hull(), mirror(), the '#' (highlight) and '%' (background) modifiers,
// and a faceted-prism cylinder ($fn=6).

module pillar(d = 4, h = 12, marked = false) {
    if (marked) #cylinder(d = d, h = h, $fn = 6);
    else cylinder(d = d, h = h, $fn = 6);
}

for (i = [0 : 2 : 8])
    translate([i * 6, 0, 0]) pillar(d = 3 + i / 4, h = 10, marked = (i == 4));

hull() {
    translate([0, 20, 0]) cylinder(r = 2, h = 1);
    translate([40, 20, 0]) cylinder(r = 5, h = 1);
}

mirror([0, 1, 0]) translate([0, 10, 0]) cube([50, 4, 4]);
