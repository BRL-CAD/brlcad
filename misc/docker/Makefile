.PHONY: all
all: brlcad-dev brlcad-user
build: brlcad-dev brlcad-user

brlcad-dev: dev/Dockerfile
	docker build -t brlcad-dev:latest dev

brlcad-user: user/Dockerfile
	docker build -t brlcad-user:latest user

.PHONY: tag push
tag:
	docker tag brlcad-user:latest erikgreenwald/brl-cad:latest
	docker tag brlcad-dev:latest erikgreenwald/brl-cad-dev:latest
push: tag
	docker push erikgreenwald/brl-cad:latest
	docker push erikgreenwald/brl-cad-dev:latest
