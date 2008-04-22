#include <iostream>
#include "n_iges.hpp"
#include "brlcad.hpp"

using namespace brlcad;

int
main(int argc, char** argv) {
  cout << argc << endl;
  if (argc != 3) {
    bu_exit(0, "iges <iges_filename> <output_filename>\n");
  }

  string file(argv[1]);
  IGES iges(file);

  BRLCADBrepHandler bh;
  iges.readBreps(&bh);
  string out(argv[2]);
  bh.write(out);

  return 0;
}
