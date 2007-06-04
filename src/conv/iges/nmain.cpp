#include "n_iges.hpp"
#include "brlcad.hpp"

using namespace brlcad;

int
main(int argc, char** argv) {
  IGES iges("/home/jlowens/test_box.igs");

  BRLCADBrepHandler bh;
  iges.readBreps(&bh);
  bh.write();
  
  return 0;
}
