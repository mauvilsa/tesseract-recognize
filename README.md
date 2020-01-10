# NAME

tesseract-recognize - A tool that does layout analysis and/or text recognition using tesseract and outputs the result in Page XML format.

[![Docker Automated build](https://img.shields.io/docker/build/mauvilsa/tesseract-recognize.svg)]()


# INSTALLATION AND USAGE

If you want to compile from source follow the instructions here. But if you only
want to use the tools it is better to use docker as explained in the next
section.

    git clone --recursive https://github.com/mauvilsa/tesseract-recognize
    mkdir tesseract-recognize/build
    cd tesseract-recognize/build
    cmake -DCMAKE_INSTALL_PREFIX:PATH=$HOME ..
    make install
    
    tesseract-recognize --help
    tesseract-recognize IMAGE1 IMAGE2 -o OUTPUT.xml
    tesseract-recognize INPUT.xml -o OUTPUT.xml


# INSTALLATION AND USAGE (DOCKER)

The latest docker images are based on Ubuntu 18.04 and use the version of
tesseract from the default package repositories (see the respective [docker hub
page](https://hub.docker.com/r/mauvilsa/tesseract-recognize/)).

The docker images only include language files for recognition of English, so for
additional languages you need to get the corresponding files and make them
accessible to the container. To install first pull the docker image of your
choosing, using a command such as:

    TAG="SELECTED_TAG_HERE"
    docker pull mauvilsa/tesseract-recognize:$TAG

Then there are two possible ways of using it, through a command line interface
or through a REST API.

## Command line interface

First download the
[https://github.com/omni-us/docker-command-line-interface](docker-cli), put it
in some directory in your path and make it executable, for example:

    wget -O $HOME/.local/bin https://raw.githubusercontent.com/omni-us/docker-command-line-interface/master/docker-cli
    chmod +x $HOME/.local/bin/docker-cli

As an additional step, you could look at `docker-cli --help` and read about how
to configure bash completion.

After installing docker-cli, the tesseract-recognize tool can be used like any
other command, i.e.

    docker-cli --ipc=host -- mauvilsa/tesseract-recognize:$TAG tesseract-recognize --help
    docker-cli --ipc=host -- mauvilsa/tesseract-recognize:$TAG tesseract-recognize IMAGE -o OUTPUT.xml

For convenience you could setup an alias, i.e.

    alias tesseract-recognize-docker="docker-cli --ipc=host -- mauvilsa/tesseract-recognize:$TAG tesseract-recognize"
    tesseract-recognize-docker --help

## API interface

The API interface uses a python flask sever that can be accessed through port
5000 inside the docker container. For example the server could be started as:

    docker run --rm -t -p 5000:5000 mauvilsa/tesseract-recognize:$TAG 

The API exposes the following endpoints:

Method | Endpoint                          | Description                      | Parameters (form fields)
------ | --------------------------------- | -------------------------------- | ------------------------
GET    | /tesseract-recognize/version      | Returns tool version information | -
GET    | /tesseract-recognize/help         | Returns tool help                | -
GET    | /tesseract-recognize/swagger.json | The swagger json                 | -
GET    | /tesseract-recognize/openapi.json | The openapi json                 | -
POST   | /tesseract-recognize/process      | Recognize given images or xml    | **images (array, required):** Image files with names as in page xml. **pagexml (optional):** Page xml file to recognize. **options (optional):** Array of strings with options for the tesseract-recognize tool.

For illustration purposes the curl command can be used. Processing an input
image with a non-default layout level would be using a POST such as

    curl -o output.xml -F images=@img.png -F options='["--layout", "word"]' http://localhost:5000/tesseract-recognize/process

To process a page xml file, both the xml and the respective images should be
included in the request, that is for example

    curl -o output.xml -F images=@img1.png -F images=@img2.png -F pagexml=input.xml http://localhost:5000/tesseract-recognize/process

The API is implemented using Flask-RESTPlus which allows that once the server is
started, you can use a browser to get a more detailed view of the exposed
endpoints by going to http://localhost:5000/tesseract-recognize/swagger.


# VIEWING RESULTS

The results can be viewed/edited using the Page XML editor available at
https://github.com/mauvilsa/nw-page-editor or using other tools that support
this format such as http://www.primaresearch.org/tools and
https://transkribus.eu/Transkribus/ .


# CONTRIBUTING

If you intend to contribute, before any commits be sure to first execute
githook-pre-commit to setup (symlink) the pre-commit hook. This hook takes care
of automatically updating the tool version.


# COPYRIGHT

The MIT License (MIT)

Copyright (c) 2015-present, Mauricio Villegas <mauricio_ville@yahoo.com>
