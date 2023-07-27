from ubuntu:latest

RUN apt-get -q update -y
RUN apt-get -q dist-upgrade -y
RUN useradd -rm -d /home/cadling -s /bin/bash -g root -u 1001 cadling
RUN echo root:brlcad | chpasswd
RUN echo cadling:brlcad | chpasswd
RUN DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt-get -qy install tzdata openssh-server xauth libopengl0 tk libqt5widgets5 libpng16-16
RUN service ssh start

# BRL-CAD
copy BRL-CAD_7.34.3_Linux_x86_64.tar.gz /BRL-CAD_7.34.3_Linux_x86_64.tar.gz

RUN DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt-get -qy install openbox menu tigervnc-standalone-server
RUN echo 'export PATH=$PATH:/opt/brlcad/bin' >> /etc/bash.bashrc
RUN su cadling -c 'touch /home/cadling/.Xauthority'
RUN su cadling -c 'mkdir /home/cadling/.vnc/'
RUN su cadling -c 'echo brlcad | vncpasswd -f > /home/cadling/.vnc/passwd'
RUN su cadling -c 'chmod 600 /home/cadling/.vnc/passwd'
RUN mkdir -p /opt/brlcad/
RUN ln -s /opt/brlcad/BRL-CAD_7.34.3_$(uname -s)_$(uname -m)/bin /opt/brlcad/
RUN ln -s /opt/brlcad/BRL-CAD_7.34.3_$(uname -s)_$(uname -m)/include /opt/brlcad/
RUN ln -s /opt/brlcad/BRL-CAD_7.34.3_$(uname -s)_$(uname -m)/libexec /opt/brlcad/
RUN ln -s /opt/brlcad/BRL-CAD_7.34.3_$(uname -s)_$(uname -m)/lib /opt/brlcad/
RUN ln -s /opt/brlcad/BRL-CAD_7.34.3_$(uname -s)_$(uname -m)/share /opt/brlcad/
RUN tar -zxf BRL-CAD_7.34.3_Linux_x86_64.tar.gz -C /opt/brlcad/

copy startup /startup
copy xinit /xinit
RUN chmod 555 /startup /xinit
run /bin/rm -f /BRL-CAD_7.34.3_Linux_x86_64.tar.gz

cmd ["/bin/sh", "-x", "/startup"]
