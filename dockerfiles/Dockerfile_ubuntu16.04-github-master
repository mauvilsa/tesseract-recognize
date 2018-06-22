FROM library/ubuntu:16.04

MAINTAINER Mauricio Villegas <mauricio_ville@yahoo.com>

ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-c"]


### Copy the source code to a temporal directory ###
COPY . /tmp/tesseract-recognize/


### Install build pre-requisites ###
RUN apt-get update --fix-missing \
 && apt-get install -y --no-install-recommends \
      ca-certificates \
      build-essential \
      cmake \
      git \
      libxml2-dev \
      libxslt1-dev \
      libopencv-dev \
      libmagick++-dev \
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
 && git clone https://github.com/tesseract-ocr/tesseract.git /tmp/tesseract \
 && cd /tmp/tesseract \
 && v=$(git log --date=iso -1 | sed -n '/^Date:/{s|^Date: *||;s| .*||;s|-|.|g;p;}') \
 && sed -i "s|TESSERACT_VERSION_STR .*|TESSERACT_VERSION_STR \"$v\"|" ccutil/version.h \
 && ./autogen.sh \
 && ./configure --prefix=/usr \
 #&& CFLAGS="-O2 -DUSE_STD_NAMESPACE" ./configure --prefix=/usr \
 && make -j$(nproc) \
 && echo "tesseract-ocr" > description-pak \
 && checkinstall -y --pkgname=tesseract-ocr --maintainer=mauricio_ville@yahoo.com --pkgversion=$v --pkgrelease=0 \


### Compile and install tesseract-recognize ###
 && cd /tmp/tesseract-recognize \
 && cmake -DCMAKE_BUILD_TYPE=Release . \
 && make install install-docker \


### Remove build-only software and install runtime pre-requisites ###
 && cd \
 && rm -rf /tmp/tesseract-recognize /tmp/tesseract \
 && apt-get purge -y \
      ca-certificates \
      build-essential \
      cmake \
      git \
      libxml2-dev \
      libxslt1-dev \
      libopencv-dev \
      libmagick++-dev \
      libtool \
      automake \
      autoconf \
      autoconf-archive \
      checkinstall \
 && apt-get autoremove -y \
 && apt-get purge -y $(dpkg -l | awk '{if($1=="rc")print $2}') \
 && apt-get install -y --no-install-recommends \
      ghostscript \
      libxml2 \
      libxslt1.1 \
      libopencv-core2.4v5 \
      libmagick++-6.q16-5v5 \
      libgomp1 \
      python-flask \
      python-six \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/*


### By default start the flask API server ###
CMD ["/usr/bin/python","/usr/local/bin/tesseract_recognize_api.py"]
EXPOSE 5000
