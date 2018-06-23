# NAME

tesseract-recognize - A tool that does layout analysis and/or text recognition using tesseract and outputs the result in Page XML format.

[![Docker Automated build](https://img.shields.io/docker/build/mauvilsa/tesseract-recognize.svg)]()


# INSTALLATION AND USAGE

    git clone https://github.com/mauvilsa/tesseract-recognize
    mkdir tesseract-recognize/build
    cd tesseract-recognize/build
    cmake -DCMAKE_INSTALL_PREFIX:PATH=$HOME ..
    make install
    
    tesseract-recognize --help
    tesseract-recognize IMAGE OUTPUT.xml


# INSTALLATION AND USAGE (DOCKER)

The latest docker images are based on Ubuntu 18.04 and use the version of tesseract from the default package repositories (see the respective [docker hub page](https://hub.docker.com/r/mauvilsa/tesseract-recognize/)).

The docker images do not include language files for recognition, so additional to the docker image you need to get the corresponding files and make them accessible to the container. To install first pull the docker image of your choosing, using a command such as:

    docker pull mauvilsa/tesseract-recognize:TAG

Then there are two possible ways of using it, through a command line interface or through a REST API.

## Command line interface

First you need to copy the command line interface script to some directory in your path.

    docker run --rm -it -u $(id -u):$(id -g) -v $HOME:$HOME mauvilsa/tesseract-recognize:TAG bash -c "cp /usr/local/bin/tesseract-recognize-docker $HOME/bin"

Then the docker image needs to be tagged as tesseract-recognize:active to indicate to the command line interface which is the docker image that has to be used.

    docker tag mauvilsa/tesseract-recognize:TAG tesseract-recognize:active

After this, the tool can be used like any other command, i.e.

    tesseract-recognize-docker --help
    tesseract-recognize-docker IMAGE OUTPUT.xml

## API interface

The API interface uses a python flask sever that can be accessed through port 5000 inside the docker container. A volume needs to be shared with the container for input and output files and the container has to be started as a user with write permissions to that directory. For example the server could be started as:

    docker run --rm -t -p 5000:5000 -v $(pwd):/DATA -u $(id -u):$(id -g) mauvilsa/tesseract-recognize:TAG 

Then tesseract-recognize instances can be executed through HTTP with the following endpoint:

Method | Endpoint             | Parameters (json object)
------ | -------------------- | ------------------------
POST   | /tesseract-recognize | **input_file (required):** String with the path to the input file. **output_file (required):** String with the path where to write the output file. **options (optional):** Array of strings with options for the tesseract-recognize tool.

For illustration purposes the curl command can be used. Processing an input image would be using a POST such as

    curl -s -X POST -H "Content-Type: application/json" -d '{"input_file":"/DATA/img.png","output_file":"/DATA/img3.xml"}' http://localhost:5000/tesseract-recognize

The API can also be accessed just to request the help of tesseract-recognize:

    curl -s -X POST -H "Content-Type: application/json" -d '{"options":["--help"]}' http://localhost:5000/tesseract-recognize


# VIEWING RESULTS

The results can be viewed/edited using the Page XML editor available at https://github.com/mauvilsa/nw-page-editor or using other tools that support this format such as http://www.primaresearch.org/tools and https://transkribus.eu/Transkribus/ .


# CONTRIBUTING

If you intend to contribute, before any commits be sure to first execute githook-pre-commit to setup (symlink) the pre-commit hook. This hook takes care of automatically updating the tool version.


# COPYRIGHT

The MIT License (MIT)

Copyright (c) 2015-present, Mauricio Villegas <mauricio_ville@yahoo.com>
