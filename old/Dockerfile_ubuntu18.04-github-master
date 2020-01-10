FROM library/ubuntu:18.04

MAINTAINER Mauricio Villegas <mauricio_ville@yahoo.com>

ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-c"]


### Copy the source code to a temporal directory ###
COPY . /tmp/tesseract-recognize/


### Install build pre-requisites ###
RUN apt-get update --fix-missing \
 #&& d=$(apt-cache depends libopencv-dev | sed -n '/Depends: libopencv-.*-dev/{ s|.* ||; s|$|-|; p; }' | grep -v libopencv-core-dev | tr '\n' ' ') \
 && apt-get install -y --no-install-recommends \
      ca-certificates \
      build-essential \
      cmake \
      git \
      libxml2-dev \
      libxslt1-dev \
      libopencv-dev \
      libmagick++-dev \
      libleptonica-dev \
      libtool \
      automake \
      autoconf \
      autoconf-archive \
      #checkinstall \
 #&& d=$(apt-cache depends libopencv-dev | sed -n '/Depends: libopencv-.*-dev/{ s|.* ||; p; }' | grep -v libopencv-core-dev | tr '\n' ' ') \
 && dpkg -r --force-depends libtesseract4 \


### Compile and install latest tesseract from repo ###
 && git clone https://github.com/tesseract-ocr/tesseract.git /tmp/tesseract \
 && cd /tmp/tesseract \
 && ./autogen.sh \
 && ./configure --prefix=/usr \
 #&& CFLAGS="-O2 -DUSE_STD_NAMESPACE" ./configure --prefix=/usr \
 #&& v=$(git log --date=iso -1 | sed -n '/^Date:/{s|^Date: *||;s| .*||;s|-|.|g;p;}') \
 #&& sed -i "s|TESSERACT_VERSION_STR .*|TESSERACT_VERSION_STR \"$v\"|" src/api/tess_version.h \
 && make -j$(nproc) \
 && make install \
# && echo "tesseract-ocr" > description-pak \
## && checkinstall -y --pkgname=tesseract-ocr --maintainer=mauricio_ville@yahoo.com --pkgversion=$v --pkgrelease=0 \
# && checkinstall -y --pkgname=tesseract-ocr --maintainer=mauricio_ville@yahoo.com --pkgrelease=0 \


### Compile and install tesseract-recognize ###
 && cd /tmp/tesseract-recognize \
 && cmake -DCMAKE_BUILD_TYPE=Release . \
 && make install install-docker \


### Remove build-only software and install runtime pre-requisites ###
 && cd \
 && rm -rf /tmp/tesseract-recognize /tmp/tesseract \
 && apt --fix-broken -y install \
 && apt-get purge -y --fix-broken \
      ca-certificates \
      build-essential \
      cmake \
      git \
      libxml2-dev \
      libxslt1-dev \
      libopencv-dev \
      libmagick++-dev \
      libleptonica-dev \
      libtool \
      automake \
      autoconf \
      autoconf-archive \
      #checkinstall \
 && apt-get autoremove -y \
 && apt-get purge -y $(dpkg -l | awk '{if($1=="rc")print $2}') \
 && apt-get install -y --no-install-recommends \
      ghostscript \
      libxml2 \
      libxslt1.1 \
      libopencv-core3.2 \
      libmagick++-6.q16-7 \
      liblept5 \
      libgomp1 \
      python-flask \
      python-six \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/*


### By default start the flask API server ###
CMD ["/usr/bin/python","/usr/local/bin/tesseract_recognize_api.py"]
EXPOSE 5000
