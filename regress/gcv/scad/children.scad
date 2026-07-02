// Operator modules that consume children() and report $children.
// Exercises: user-defined operator modules, children(), $children,
// nested for with multiple iterators, and per-instance $fn override.

module grid(nx = 3, ny = 2, spacing = 15) {
    for (x = [0 : nx - 1], y = [0 : ny - 1])
        translate([x * spacing, y * spacing, 0]) children();
    echo(num_children = $children);
}

grid(3, 2) {
    cube(5, center = true);
}

translate([0, 0, 30]) grid(2, 2, 20) sphere(r = 3, $fn = 24);
