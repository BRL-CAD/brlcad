#include "n_iges.hpp"

using namespace brlcad;

int
main(int argc, char** argv) {
  IGES iges("/home/jlowens/piston_head.igs");

  BrepHandler bh;
  iges.readBreps(bh);
  
  return 0;
}
