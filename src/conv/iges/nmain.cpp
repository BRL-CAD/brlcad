#include <iostream>
#include "n_iges.hpp"
#include "brlcad.hpp"

using namespace brlcad;

int
main(int argc, char** argv) {
  cout << argc << endl;
  if (argc != 2) {
    cerr << "need an IGES file..." << endl;
    exit(0);
  } 
  
  string file(argv[1]);
  IGES iges(file);

  BRLCADBrepHandler bh;
  iges.readBreps(&bh);
  bh.write();
  
  return 0;
}
