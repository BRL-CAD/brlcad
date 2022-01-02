# common
from brlcad-common:latest

copy build-dev.sh /build-dev.sh
copy /startup /startup
run /bin/bash -xe /build-dev.sh
run /bin/rm -f /build-dev.sh

cmd ["/bin/bash","-x","/startup"]
