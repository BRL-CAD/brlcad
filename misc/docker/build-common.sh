apt-get -q update -y
apt-get -q dist-upgrade -y
useradd -rm -d /home/cadling -s /bin/bash -g root -u 1001 cadling
echo root:brlcad | chpasswd
echo cadling:brlcad | chpasswd
DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt-get -qy install tzdata openssh-server xauth libopengl0 tk libqt5widgets5 libpng16-16
service ssh start
