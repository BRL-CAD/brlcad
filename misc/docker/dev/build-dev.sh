DEBIAN_FRONTEND=noninteractive /usr/bin/apt-get -qy install cmake git build-essential ninja-build swig doxygen libpng-dev libgl1-mesa-dev libopenmpi-dev libqt5opengl5-dev libeigen3-dev itk3-dev time npm
npm install -g @antora/cli @antora/site-generator-default
npm install -g antora-site-generator-lunr

mkdir -m 755 /brlcad
chown -R cadling /brlcad
chmod 555 /startup

su cadling -c '/usr/bin/time -v /usr/bin/git clone https://github.com/BRL-CAD/brlcad.git /brlcad'
su cadling -c '/usr/bin/time -v /usr/bin/cmake -G Ninja -S /brlcad/ -B /brlcad/build/ -DBRLCAD_ENABLE_QT=ON'
