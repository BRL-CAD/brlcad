﻿Since there are no Input validation of the data given for triangulation you need
to think about this. Poly2Tri does not support repeat points within epsilon.

* If you have a cyclic function that generates random points make sure you don't
  add the same coordinate twice.
* If you are given input and aren't sure same point exist twice you need to
  check for this yourself.
* Only simple polygons are supported. You may add holes or interior Steiner points
* Interior holes must not touch other holes, nor touch the polyline boundary
* Use the library in this order:
  1. Initialize CDT with a simple polyline (this defines the constrained edges)
  2. Add holes if necessary (also simple polylines)
  3. Add Steiner points
  4. Triangulate

Make sure you understand the preceding notice before posting an issue. If you have
an issue not covered by the above, include your data-set with the problem.
The only easy day was yesterday; have a nice day. <Mason Green>

TESTBED INSTALLATION GUIDE
==========================

Dependencies
------------

Core poly2tri lib:

* Standard Template Library (STL)

Testbed:

* gcc
* OpenGL
* [GLFW](http://glfw.sf.net)
* Python

[waf](http://code.google.com/p/waf/) is used to compile the testbed.
A waf script (86kb) is included in the repositoty.

Building the Testbed
--------------------

Posix/MSYS environment:
```
./waf configure
./waf build
```

Windows command line:
```
python waf configure
python waf build
```

Running the Examples
--------------------

Load data points from a file:
```
p2t <filename> <center_x> <center_y> <zoom>
```
Random distribution of points inside a consrained box:
```
p2t random <num_points> <box_radius> <zoom>
```
Examples:
```
./p2t dude.dat 300 500 2
./p2t nazca_monkey.dat 0 0 9

./p2t random 10 100 5.0
./p2t random 1000 20000 0.025
```