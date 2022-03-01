# Docker image for BRL-CAD development

start with:
	make all
	docker-compose -p brlcad up -d --build

opens ssh on port 4322. Source is checked out to /brlcad and obj dir configured in /brlcad/build.

User: cadling
User password: brlcad
Root password: brlcad

from the vs code remote extension:

	ssh -ACX -l cadling -p 4322 localhost

then install various extensions, like cmake, c++, doxygen, etc.

For other user cases, it might be better to mount a local fs (volume to dir in docker-compose.yaml) or NFS mount and have a user with the appropriate uid.
