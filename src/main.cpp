#include<iostream>
#include<cstdlib>
#include<string>

// /brlcad/build/share/db/moss.g
// /brlcad/src/gtools/rgen/visualization
// ../../../../../build/bin/mged ../db/moss.g tops
// ../../../../../build/bin/rt ../db/moss.g all.g

int main(int argc, char **argv) {
    if (argc == 2) {
        const char* filename = argv[1];
        std::cout << "Processing file: " << filename << std::endl;
        const char* mgedTops = "../../../../../build/bin/mged ../db/moss.g tops";
        auto result1 = system(mgedTops);
        const char* rtAll = "../../../../../build/bin/rt -C 255/255/255 -s 1024 -c \"set ambSamples=64\" ../db/moss.g all.g";
        auto result2 = system(rtAll);
        std::cout << "Success\n";
    } else {
        return 1;
    }
}

