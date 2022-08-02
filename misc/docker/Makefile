.PHONY: all
all: dev user
build: common dev user

common: Dockerfile build-common.sh
	docker build -t brlcad-common:latest .

dev: dev/Dockerfile dev/build-dev.sh common
	docker build -t brlcad-dev:latest dev

user: user/Dockerfile user/build-user.sh common
	docker build -t brlcad-user:latest user

.PHONY: tag push
tag:
	docker tag brlcad-user:latest erikgreenwald/brl-cad:latest
	docker tag brlcad-dev:latest erikgreenwald/brl-cad-dev:latest
push: tag
	docker push erikgreenwald/brl-cad:latest
	docker push erikgreenwald/brl-cad-dev:latest
