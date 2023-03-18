from ubuntu:latest

RUN apt-get -q update -y
RUN apt-get -q dist-upgrade -y
RUN useradd -rm -d /home/cadling -s /bin/bash -g root -u 1001 cadling
RUN echo root:brlcad | chpasswd
RUN echo cadling:brlcad | chpasswd
RUN DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt-get -qy install tzdata openssh-server xauth libopengl0 tk libqt5widgets5 libpng16-16
RUN service ssh start

RUN DEBIAN_FRONTEND=noninteractive /usr/bin/apt-get -qy install cmake git build-essential ninja-build swig doxygen libpng-dev libgl1-mesa-dev libopenmpi-dev libqt5opengl5-dev libeigen3-dev itk3-dev time npm
RUN npm install -g @antora/cli @antora/site-generator-default
RUN npm install -g antora-site-generator-lunr

RUN mkdir -m 755 /brlcad
RUN chown -R cadling /brlcad
COPY startup /startup
RUN chmod 555 /startup

RUN su cadling -c '/usr/bin/time -v /usr/bin/git clone https://github.com/BRL-CAD/brlcad.git /brlcad'
RUN su cadling -c '/usr/bin/time -v /usr/bin/cmake -G Ninja -S /brlcad/ -B /brlcad/build/ -DBRLCAD_ENABLE_QT=ON'

copy /startup /startup

cmd ["/bin/bash","-x","/startup"]
