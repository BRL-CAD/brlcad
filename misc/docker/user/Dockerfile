# common
from brlcad-common:latest

# BRL-CAD
copy BRL-CAD_7.32.5_Linux_x86_64.tar.gz /BRL-CAD_7.32.5_Linux_x86_64.tar.gz 
copy build-user.sh /build-user.sh
copy startup /startup
copy xinit /xinit
run /bin/bash -xe /build-user.sh
run /bin/rm -f /build-user.sh /BRL-CAD_7.32.5_Linux_x86_64.tar.gz

cmd ["/bin/sh", "-x", "/startup"]
