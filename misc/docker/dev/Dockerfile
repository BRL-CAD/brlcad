# common
from ubuntu:latest
run apt-get update -y
run apt-get dist-upgrade -y
run useradd -rm -d /home/cadling -s /bin/bash -g root -u 1001 cadling
run echo root:brlcad | chpasswd
run echo cadling:brlcad | chpasswd
run DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt-get -y install tzdata openssh-server xauth libopengl0 tk libqt5widgets5 libpng16-16
run service ssh start

run DEBIAN_FRONTEND=noninteractive /usr/bin/apt-get -y install cmake git build-essential ninja-build swig doxygen libpng-dev libgl1-mesa-dev libopenmpi-dev libqt5opengl5-dev libeigen3-dev itk3-dev

run mkdir -m 755 /brlcad
run chown -R cadling /brlcad
copy startup /startup
run chmod 555 /startup

user cadling
run /usr/bin/git clone https://github.com/BRL-CAD/brlcad.git /brlcad
run /usr/bin/cmake -G Ninja -S /brlcad/ -B /brlcad/build -DBRLCAD_ENABLE_QT=ON

user root
cmd ["/bin/bash","-x","/startup"]
