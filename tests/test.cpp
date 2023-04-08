#include<bits/stdc++.h>
#include "ged.h"
using namespace std;

int main() {
    std::string retrieveSub = "\"foreach {s} \\[ lt " + "all.g" + " \\] { set o \\[lindex \\$s 1\\] ; puts \\\"\\$o \\[llength \\[search \\$o \\] \\] \\\" }\" ";
    cout << retrieveSub << endl;
    const char* cmd[2] = { retrieveSub.c_str(), NULL };
    ged_exec(g, 1, cmd);
    cout << bu_vls_addr(g->ged_result_str) << endl;
}
