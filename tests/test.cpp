#include<bits/stdc++.h>
#include<filesystem>
//#include "ged.h"
using namespace std;

int main() {
    string outputname = "../output/m35_misc_bed_frame__ghost.png";
    if (std::filesystem::exists(outputname.c_str())) {
        if (std::remove(outputname.c_str()) != 0) {
            std::cerr << "File: " << outputname << " could not be removed." << std::endl;
        }
    } else {
        std::cerr << "File: " << outputname << " does not exist." << std::endl;
    }
}
