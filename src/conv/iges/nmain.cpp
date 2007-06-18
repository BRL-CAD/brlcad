#include <iostream>
#include "n_iges.hpp"
#include "brlcad.hpp"

using namespace brlcad;

int
main(int argc, char** argv) {
  cout << argc << endl;
  if (argc != 3) {
    cerr << "iges <iges_filename> <output_filename>" << endl;
    exit(0);
  } 
  
  string file(argv[1]);
  IGES iges(file);

  BRLCADBrepHandler bh;
  iges.readBreps(&bh);
  string out(argv[2]);
  bh.write(out);
  
  return 0;
}
