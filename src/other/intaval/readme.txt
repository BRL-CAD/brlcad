(replacement for a manual page)

Usage:
	tgf-g.exe <INTAVAL material file name> <INTAVAL geometry file name> <BRL-CAD file name (will be created)>

The conversion will transfer the primitives in the following way:
	Long Form -----------> Bag of Triangles (Solid Mode)
	Two-Point Box -------> Rectangular Parallelepiped
	Three-Point Box -----> Truncated Right Circular Cone
	Five-Point Box ------> Right Circular Cylinder
	Eight-Point Box -----> Arbitrary Polyhedron with 8 Vertices
	Zero-Point Box ------> (Translation of Previous Solid)
	One-Point Box -------> Right Circular Cylinder
	Minus One-Point Box -> Pipe
	Minus N Point Box ---> Bag of Triangles (Plate Mode)
	Ring Mode Box -------> Bag of Triangles (Plate Mode)
	CAD Type Box --------> Bag of Triangles (Solid Mode)

TGF is an INTAVAL Target Geometry File
DRA is the Defence Research Agency established by the UK Ministry of Defence
