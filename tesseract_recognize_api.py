#!/usr/bin/env python

""" API implementation for the tesseract-recognize microservice
  @version $Version: 2017.10.11$
  @author Mauricio Villegas <mauricio_ville@yahoo.com>
  @copyright Copyright(c) 2017-present, Mauricio Villegas <mauricio_ville@yahoo.com>
"""

import sys
import os.path
import logging
import traceback
from subprocess import Popen, PIPE, STDOUT
from flask import Flask, json, request, Response
from six import string_types

app = Flask(__name__) # pylint: disable=invalid-name

app.logger.addHandler(logging.StreamHandler(sys.stderr))
app.logger.setLevel(logging.INFO)

### Helper class and functions for API responses ###

class Resp(Response): # pylint: disable=too-many-ancestors
    """ Extension of Response class with automatic json support """
    def __init__(self, content=None, *args, **kargs):
        if isinstance(content, (list, dict)):
            kargs['mimetype'] = 'application/json'
            content = json.dumps(content)
        super(Resp, self).__init__(content, *args, **kargs)

    @classmethod
    def force_type(cls, response, environ=None):
        if isinstance(response, (list, dict)):
            return cls(response)
        return super(Resp, cls).force_type(response, environ)

def resp200(msg):
    """ Response when execution successful """
    return Resp({'message':msg, 'success':True})
def resp400(msg):
    """ Response when there is a problem with the request """
    app.logger.error(msg)
    return Resp({'message':msg, 'success':False}, status=400)
def resp500(msg):
    """ Response when there is a server problem """
    app.logger.error(msg)
    return Resp({'message':msg, 'success':False}, status=500)

### Definition of endpoints ###

@app.route('/tesseract-recognize', methods=['POST'])
def tesseract_recognize(): # pylint: disable=too-many-return-statements
    """ Endpoint for executing tesseract-recognize on a file """
    try:
        data = request.json

        ### Check input file(s) ###
        if 'input_file' not in data or \
           not isinstance(data['input_file'], string_types):
            return resp400('expected input_file as a string')
        if not os.path.isfile(data['input_file']):
            return resp400('input file not found: '+data['input_file'])

        ### Check output file ###
        if 'output_file' not in data or \
           not isinstance(data['output_file'], string_types):
            return resp400('expected output_file as a string')

        ### Generate command list with additional options if present ###
        cmd = ['/usr/local/bin/tesseract-recognize', data['input_file'], data['output_file']]
        if 'options' in data:
            if not isinstance(data['options'], list) or \
               not all(isinstance(i, string_types) for i in data['options']):
                return resp400('expected options as an array of strings')
            cmd.extend(data['options'])

        ### Execute tesseract-recognize command ###
        proc = Popen(cmd, shell=False, stdin=PIPE, stdout=PIPE, stderr=STDOUT, close_fds=True)
        cmd_out = str(proc.stdout.read())
        proc.communicate()
        cmd_rc = proc.returncode

        ### Response depending on the case ###
        msg = 'command='+cmd+' output='+cmd_out
        if cmd_rc != 0:
            return resp400('execution failed: '+msg+' return_code='+str(cmd_rc))
        return resp200('execution successful: '+msg)

    ### Catch any problem and respond a accordingly ###
    except Exception as ex: # pylint: disable=broad-except
        return resp500(str(ex)+"\n"+traceback.format_exc())

### Run app if script called directly ###
if __name__ == '__main__':
    app.run(host='0.0.0.0')
