FROM library/ubuntu:16.04

MAINTAINER Mauricio Villegas <mauricio_ville@yahoo.com>

ENV DEBIAN_FRONTEND=noninteractive


### Copy the source code to a temporal directory ###
COPY . /tmp/tesseract-recognize/


### Install build pre-requisites ###
RUN apt-get update --fix-missing \
 && apt-get install -y --no-install-recommends \
      build-essential \
      cmake \
      tesseract-ocr-dev \
      libleptonica-dev \
      libxml2-dev \
      libxslt1-dev \
      libopencv-dev \
      libmagick++-dev \


### Compile and install tesseract-recognize ###
 && cd /tmp/tesseract-recognize \
 && cmake -DCMAKE_BUILD_TYPE=Release . \
 && make install install-docker \


### Remove build-only software and install runtime pre-requisites ###
 && cd \
 && rm -rf /tmp/tesseract-recognize \
 && apt-get purge -y \
      build-essential \
      cmake \
      tesseract-ocr-dev \
      libleptonica-dev \
      libxml2-dev \
      libxslt1-dev \
      libopencv-dev \
      libmagick++-dev \
 && apt-get autoremove -y \
 && apt-get purge -y $(dpkg -l | awk '{if($1=="rc")print $2}') \
 && apt-get install -y --no-install-recommends \
      tesseract-ocr \
      ghostscript \
      libxml2 \
      libxslt1.1 \
      libopencv-core2.4v5 \
      libmagick++-6.q16-5v5 \
      python-flask \
      python-six \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/*


### By default start the flask API server ###
CMD ["/usr/bin/python","/usr/local/bin/tesseract_recognize_api.py"]
EXPOSE 5000
