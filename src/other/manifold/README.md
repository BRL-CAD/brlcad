# Manifold

NOTE:  This repo is just making a single-library build of the upstream manifold.  For main development work please see https://github.com/elalish/manifold

[Manifold](https://github.com/elalish/manifold) is a geometry library dedicated to creating and operating on manifold triangle meshes. A [manifold mesh](https://github.com/elalish/manifold/wiki/Manifold-Library#manifoldness) is a mesh that represents a solid object, and so is very important in manufacturing, CAD, structural analysis, etc. Further information can be found on the [wiki](https://github.com/elalish/manifold/wiki/Manifold-Library).

Dependencies:
- [`GLM`](https://github.com/g-truc/glm/): A compact header-only vector library.
- [`Thrust`](https://github.com/NVIDIA/thrust): NVIDIA's parallel algorithms library (basically a superset of C++17 std::parallel_algorithms)
- [`Clipper2`](https://github.com/AngusJohnson/Clipper2): provides our 2D subsystem
- [`quickhull`](https://github.com/akuukka/quickhull): 3D convex hull algorithm.

