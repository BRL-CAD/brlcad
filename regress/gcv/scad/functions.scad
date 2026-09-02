// User functions, recursion, expressions, list comprehensions, builtins.
// Exercises: function definitions, recursion (fib), ternary, let(),
// list comprehension over a range, vector indexing and .x/.y swizzles,
// and the builtins sin/cos/len/concat/norm/sqrt.

function fib(n) = n < 2 ? n : fib(n - 1) + fib(n - 2);
function ring(n, r) = [ for (i = [0 : n - 1]) let(a = i * 360 / n) [r * cos(a), r * sin(a)] ];

pts = ring(8, 20);
echo(count = len(pts), first = pts[0], firstx = pts[0].x);

for (p = pts)
    translate([p.x, p.y, 0]) sphere(r = 1 + fib(3) * 0.4);

dir = concat([1, 0], [0]);
echo(unit_len = norm(dir), root2 = sqrt(2));
