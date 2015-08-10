dom2dox copies a C++ source file, replacing Doc-O-Matic documentation comments with equivalent Doxygen comments.

Usage: dom2dox input output

Known issues:
* BRL-CAD specific.
* Contains memory leaks and missing checks.
* Removes non-documentation comments, sometimes reformatting code.
* Doesn't preserve indentation in example sections.
* Doesn't guard against documentation appearing inside string literals
* Doesn't guard against against documentation which is commented out.


to build:

build BRL-CAD (say, with source directory /home/user/brlcad and build
directory /home/user/brlcad/build)

export PATH=/home/user/brlcad/build/bin:$PATH

mkdir build
cd build
cmake .. -DBRLCAD_SRC_INCLUDE_DIR=/home/user/brlcad/include
-DBRLCAD_BUILD_INCLUDE_DIR=/home/user/brlcad/build/include
-DTCL_INCLUDE_DIR=/home/user/brlcad/src/other/tcl/generic
-DBU_LIBRARY=/home/user/brlcad/build/lib/libbu.so
make

