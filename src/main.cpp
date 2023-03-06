#include<iostream>
#include<cstdlib>
#include<string>

// /brlcad/build/share/db/moss.g
// /brlcad/src/gtools/rgen/visualization
// ../../../../../build/bin/mged ../db/moss.g tops
// ../../../../../build/bin/rt ../db/moss.g all.g

int main(int argc, char **argv) {
    if (argc == 2) {
        std::string filename = argv[1];
        std::cout << "Processing file: " << filename << std::endl;

        std::string genTops = "../../../../../build/bin/mged " + filename + " tops";
        auto result1 = system(genTops.c_str());

        std::string renderAll = "../../../../../build/bin/rt -C 255/255/255 -s 1024 -c \"set ambSamples=64\" " + filename + " all.g";
        auto result2 = system(renderAll.c_str());
        std::cout << "Success\n";
    } else {
        return 1;
    }
}

