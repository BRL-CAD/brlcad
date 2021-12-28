# common
from ubuntu:latest
run apt-get update -y
run apt-get dist-upgrade -y
run useradd -rm -d /home/cadling -s /bin/bash -g root -u 1001 cadling
run echo root:brlcad | chpasswd
run echo cadling:brlcad | chpasswd
run DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt-get -y install tzdata openssh-server xauth libopengl0 tk libqt5widgets5 libpng16-16
run service ssh start

# BRL-CAD
copy BRL-CAD_7.32.5_Linux_x86_64.tar.gz /
run mkdir -p /opt/brlcad/
run ln -s /opt/brlcad/BRL-CAD_7.32.5_$(uname -s)_$(uname -m)/bin /opt/brlcad/
run ln -s /opt/brlcad/BRL-CAD_7.32.5_$(uname -s)_$(uname -m)/include /opt/brlcad/
run ln -s /opt/brlcad/BRL-CAD_7.32.5_$(uname -s)_$(uname -m)/libexec /opt/brlcad/
run ln -s /opt/brlcad/BRL-CAD_7.32.5_$(uname -s)_$(uname -m)/lib /opt/brlcad/
run ln -s /opt/brlcad/BRL-CAD_7.32.5_$(uname -s)_$(uname -m)/share /opt/brlcad/
run tar -zxvf BRL-CAD_7.32.5_Linux_x86_64.tar.gz -C /opt/brlcad/
run echo 'export PATH=$PATH:/opt/brlcad/bin' >> /etc/bash.bashrc
run DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt-get -y install openbox menu tigervnc-standalone-server

copy startup /startup
copy xinit /xinit
run chmod 555 /startup /xinit
user cadling
run touch ~/.Xauthority
run mkdir /home/cadling/.vnc/
run echo brlcad | vncpasswd -f > ~/.vnc/passwd
run chmod 600 ~/.vnc/passwd ~/.Xauthority

user root
cmd ["/bin/sh", "-x", "/startup"]
