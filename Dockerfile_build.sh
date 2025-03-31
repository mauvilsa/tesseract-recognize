#!/usr/bin/env bash

UBUNTU="24.04"
VERSION=$(sed -r -n '/^current_version/{ s/.*= //; p; }' .bumpversion.cfg)

docker build \
  -t mauvilsa/tesseract-recognize:$VERSION-ubuntu$UBUNTU-pkg \
  -f Dockerfile-pkg \
  --build-arg UBUNTU_TAG=$UBUNTU \
  .

docker build \
  -t mauvilsa/tesseract-recognize:$VERSION-ubuntu$UBUNTU-pkg-langs \
  -f Dockerfile-pkg-langs \
  --build-arg UBUNTU_TAG=$UBUNTU \
  .
