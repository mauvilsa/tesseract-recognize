#!/usr/bin/env bash

VERSION=$(sed -r -n '/^current_version/{ s/.*= //; p; }' .bumpversion.cfg)
docker build -t mauvilsa/tesseract-recognize:$VERSION-ubuntu22.04-pkg -f Dockerfile-pkg .
docker build -t mauvilsa/tesseract-recognize:$VERSION-ubuntu22.04-pkg-langs -f Dockerfile-pkg-langs .
