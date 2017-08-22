# NAME

tesseract-recognize - A tool that does layout analysis and/or text recognition using tesseract and outputs the result in Page XML format.

# INSTALLATION AND USAGE

    git clone https://github.com/mauvilsa/tesseract-recognize
    mkdir tesseract-recognize/build
    cd tesseract-recognize/build
    cmake -DCMAKE_INSTALL_PREFIX:PATH=$HOME ..
    make install
    
    tesseract-recognize --help
    tesseract-recognize IMAGE OUTPUT.xml

# INSTALLATION AND USAGE (DOCKER)

There are two main docker images that can be chosen from (see the respective [docker hub page](https://hub.docker.com/r/mauvilsa/tesseract-recognize/)), one that uses tesseract version 3 available from the default Ubuntu repository and the other that uses the latest version found in the master branch of [github tesseract repository](https://github.com/tesseract-ocr/tesseract).

The docker images does not include language files for recognition, so additional to the docker image you need to get the corresponding files and make them accessible to the container. To install first pull the docker image of your choosing, using a command such as:

    docker pull mauvilsa/tesseract-recognize:latest

Then copy the command line interface script to some directory in your path, so that you can easily use tesseract-recognize from the host.

    docker run --rm -it -u $(id -u):$(id -g) -v $HOME:$HOME mauvilsa/tesseract-recognize:latest bash -c "cp /usr/local/bin/tesseract-recognize-docker $HOME/bin"

Finally it should work like any other command, i.e.

    tesseract-recognize-docker --help
    tesseract-recognize-docker IMAGE OUTPUT.xml

# VIEWING RESULTS

The results can be viewed/edited using the Page XML editor available at https://github.com/mauvilsa/nw-page-editor or using other tools that support this format such as http://www.primaresearch.org/tools and https://transkribus.eu/Transkribus/ .

# CONTRIBUTING

If you intend to contribute, before any commits be sure to first execute githook-pre-commit to setup (symlink) the pre-commit hook. This hook takes care of automatically updating the tool version.

# COPYRIGHT

The MIT License (MIT)

Copyright (c) 2015-present, Mauricio Villegas <mauricio_ville@yahoo.com>
