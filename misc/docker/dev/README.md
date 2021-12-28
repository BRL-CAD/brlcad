# Docker image for BRL-CAD development

start with:

	docker-compose -p brlcad up -d --build

opens ssh on port 4322. Source is checked out to /brlcad and obj dir configured in /brlcad/build.

User: cadling
User password: brlcad
Root password: brlcad

from the vs code remote extension:

	ssh -ACX -l cadling -p 4322 localhost

then install various extensions, like cmake, c++, doxygen, etc.
