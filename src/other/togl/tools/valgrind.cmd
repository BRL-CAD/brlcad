env TCLLIBPATH=. valgrind --tool=memcheck --leak-check=yes --gen-suppressions=yes --suppressions=valgrind-togl.supp /usr/local/bin/wish8.4 gears.tcl
