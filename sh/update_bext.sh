#!/bin/sh

git submodule update --remote --recursive

git add src/bext

git commit -m "update external deps to latest"

git push

