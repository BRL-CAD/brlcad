DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt-get -qy install openbox menu tigervnc-standalone-server
echo 'export PATH=$PATH:/opt/brlcad/bin' >> /etc/bash.bashrc
chmod 555 /startup /xinit
su cadling -c 'touch /home/cadling/.Xauthority'
su cadling -c 'mkdir /home/cadling/.vnc/'
su cadling -c 'echo brlcad | vncpasswd -f > /home/cadling/.vnc/passwd'
su cadling -c 'chmod 600 /home/cadling/.vnc/passwd'
mkdir -p /opt/brlcad/
ln -s /opt/brlcad/BRL-CAD_7.32.5_$(uname -s)_$(uname -m)/bin /opt/brlcad/
ln -s /opt/brlcad/BRL-CAD_7.32.5_$(uname -s)_$(uname -m)/include /opt/brlcad/
ln -s /opt/brlcad/BRL-CAD_7.32.5_$(uname -s)_$(uname -m)/libexec /opt/brlcad/
ln -s /opt/brlcad/BRL-CAD_7.32.5_$(uname -s)_$(uname -m)/lib /opt/brlcad/
ln -s /opt/brlcad/BRL-CAD_7.32.5_$(uname -s)_$(uname -m)/share /opt/brlcad/
tar -zxf BRL-CAD_7.32.5_Linux_x86_64.tar.gz -C /opt/brlcad/
