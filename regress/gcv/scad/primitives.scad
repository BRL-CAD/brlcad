// Primitives, transforms, and the three boolean operators.
// Exercises: cube, sphere, cylinder (cone), 2D polygon + linear_extrude,
// translate/rotate/scale, union/difference/intersection, color, $fn.
$fn = 32;

difference() {
    union() {
        cube([20, 20, 10], center = true);
        translate([0, 0, 5]) sphere(r = 8);
    }
    translate([0, 0, -2]) cylinder(h = 14, r = 4, center = true);
}

translate([30, 0, 0]) rotate([0, 0, 45]) scale([1, 2, 1]) cube(6);

translate([-30, 0, 0]) linear_extrude(height = 5) polygon([[0,0],[10,0],[5,8]]);

translate([0, 30, 0]) intersection() {
    sphere(10);
    cube(14, center = true);
}

color("steelblue") translate([0, -30, 0]) cylinder(h = 8, r1 = 6, r2 = 0);
