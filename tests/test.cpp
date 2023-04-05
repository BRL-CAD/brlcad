#include<bits/stdc++.h>
using namespace std;

int main() {
    int a = 45, e = 45;
    string outputname = "test.png";
    // string render = "../../../build/bin/rt -C 255/255/255 -s 1024 -A 1.5 -W -R -c \"set ambSamples=64\" -o " + outputname + " " + pathToInput + " " + component;
    string render = " ../../../build/bin/rtwizard -s 1024 -i ../../../build/share/db/m35.g -c bed -g component -G 10 -o test.png";
    string render2 = "../../../build/bin/rtwizard -s 1024 -i ../../../build/share/db/m35.g -g component -G 3 -o test1.png";
    system(render.c_str());
    system(render2.c_str());
    cout << "done!\n";
}
