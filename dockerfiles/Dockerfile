FROM library/ubuntu:16.04

MAINTAINER Mauricio Villegas <mauricio_ville@yahoo.com>

SHELL ["/bin/bash", "-c"]


### Install build pre-requisites ###
RUN apt-get update --fix-missing \
 && apt-get install -y --no-install-recommends \
      ca-certificates \
      build-essential \
      cmake \
      git \
      libxml2-dev \
      libopencv-dev \
      libtool \
      automake \
      autoconf \
      autoconf-archive \
      checkinstall \


### Install leptonica 1.74 from Ubuntu 17.10 by pinning ###
 && echo 'deb http://archive.ubuntu.com/ubuntu artful main restricted universe multiverse' > /etc/apt/sources.list.d/ubuntu17.10.list \
 && echo $'Package: *\nPin: release v=16.04, l=Ubuntu\nPin-Priority: 1000\n' > /etc/apt/preferences \
 && echo $'Package: liblept5\nPin: release v=17.10, l=Ubuntu\nPin-Priority: 1001\n' >> /etc/apt/preferences \
 && echo $'Package: libleptonica-dev\nPin: release v=17.10, l=Ubuntu\nPin-Priority: 1001' >> /etc/apt/preferences \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
      liblept5 \
      libleptonica-dev \


### Compile and install latest tesseract from repo ###
 && git clone https://github.com/tesseract-ocr/tesseract.git \
 && cd tesseract \
 && git checkout 323f26f6cc60a2ea1e7b4fd5f347406080dcf241 \
 && v=$(git log --date=iso -1 | sed -n '/^Date:/{s|^Date: *||;s| .*||;s|-|.|g;p;}') \
 && sed -i "s|TESSERACT_VERSION_STR .*|TESSERACT_VERSION_STR \"$v\"|" api/baseapi.h \
 && ./autogen.sh \
 && ./configure --prefix=/usr \
 #&& CFLAGS="-O2 -DUSE_STD_NAMESPACE" ./configure --prefix=/usr \
 && make -j$(nproc) \
 && echo "tesseract-ocr" > description-pak \
 && checkinstall -y --pkgname=tesseract-ocr --maintainer=mauricio_ville@yahoo.com --pkgversion=$v --pkgrelease=0 \


### Compile and install tesseract-recognize ###
 && cd / \
 && git clone https://github.com/mauvilsa/tesseract-recognize.git \
 && cd tesseract-recognize \
 && cmake -DCMAKE_BUILD_TYPE=Release . \
 && make install install-docker \


### Remove build-only software and install runtime pre-requisites ###
 && rm -rf /tesseract-recognize /tesseract \
 && apt-get purge -y \
      ca-certificates \
      build-essential \
      cmake \
      git \
      libxml2-dev \
      libopencv-dev \
      libtool \
      automake \
      autoconf \
      autoconf-archive \
      checkinstall \
 && apt-get autoremove -y \
 && apt-get purge -y $(dpkg -l | awk '{if($1=="rc")print $2}') \
 && apt-get install -y --no-install-recommends \
      libxml2 \
      libgomp1 \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/*
