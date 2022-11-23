# Docker image for BRL-CAD

You will need to furnish BRL-CAD\_7.32.5\_Linux\_x86.tar.gz yourself. Place it in this directory before building the docker images.

start with:
	make all
	docker-compose -p brlcad up -d --build

BRL-CAD is installed in /opt/brlcad and the path is pre-configured.

Starts a VNC server listening on port 4201 with Archer running.


also opens ssh on port 4222.

* User: cadling
* User password: brlcad
* Root password: brlcad


this little guide should probably be super user friendly instead of what it is.
