#!/usr/bin/env python3
"""Command line tool for the tesseract-recognize API server."""

"""
@version $Version: 2020.01.13$
@author Mauricio Villegas <mauricio_ville@yahoo.com>
@copyright Copyright(c) 2017-present, Mauricio Villegas <mauricio_ville@yahoo.com>

@requirements https://github.com/omni-us/pagexml/releases/download/2019.10.10/pagexml-2019.10.10-cp36-cp36m-linux_x86_64.whl
@requirements jsonargparse>=2.20.0
@requirements flask-restplus>=0.12.1
@requirements prance>=0.15.0
"""

import os
import re
import sys
import json
import shutil
import queue
import threading
import tempfile
import pagexml
pagexml.set_omnius_schema()
from time import time
from functools import wraps
from subprocess import Popen, PIPE, STDOUT
from jsonargparse import ArgumentParser, ActionConfigFile, ActionYesNo
from flask import Flask, Response, request, abort
from flask_restplus import Api, Resource, reqparse
from werkzeug.datastructures import FileStorage
from werkzeug.exceptions import BadRequest
from prance.util import url
from prance.convert import convert_url


def get_cli_parser(logger=True):
    """Returns the parser object for the command line tool."""
    parser = ArgumentParser(
        error_handler='usage_and_exit_error_handler',
        logger=logger,
        default_env=True,
        description=__doc__)

    parser.add_argument('--cfg',
        action=ActionConfigFile,
        help='Path to a yaml configuration file.')
    parser.add_argument('--threads',
        type=int,
        default=4,
        help='Maximum number of tesseract-recognize instances to run in parallel.')
    parser.add_argument('--prefix',
        default='/tesseract-recognize',
        help='Prefix string for all API endpoints. Use "%%s" in string to replace by the API version.')
    parser.add_argument('--host',
        default='127.0.0.1',
        help='Hostname to listen on.')
    parser.add_argument('--port',
        type=int,
        default=5000,
        help='Port for the server.')
    parser.add_argument('--debug',
        action=ActionYesNo,
        default=False,
        help='Whether to run in debugging mode.')

    return parser


def TypePageXML(value):
    """Parse Page XML request type.

    Args:
        value: The raw type value.

    Returns:
        dict[str, {str,PageXML}]: Dictionary including the page xml 'filename', the 'string' representation and the PageXML 'object'.
    """
    if type(value) != FileStorage:
        raise ValueError('Expected pagexml to be of type FileStorage.')

    spxml = value.read().decode('utf-8')
    pxml = pagexml.PageXML()
    pxml.loadXmlString(spxml)

    return {'filename': value.filename, 'object': pxml, 'string': spxml}


class ParserPageXML(reqparse.RequestParser):
    """Class for parsing requests including a Page XML."""

    def parse_args(self, **kwargs):
        """Extension of parse_args that additionally does some Page XML checks."""
        req_dict = super().parse_args(**kwargs)

        if req_dict['pagexml'] is not None and req_dict['images'] is not None:
            pxml = req_dict['pagexml']['object']
            images_xml = set()
            for page in pxml.select('//_:Page'):
                fname = re.sub(r'\[[0-9]+]$', '', pxml.getAttr(page, 'imageFilename'))
                images_xml.add(fname)
            images_received = [os.path.basename(x.filename) for x in req_dict['images']]
            for fname in images_received:
                if fname not in images_xml:
                    raise BadRequest('Received image not referenced in the Page XML: '+fname)
            if len(images_xml) != len(images_received):
                raise BadRequest('Expected to receive all images referenced in the Page XML ('+str(len(images_xml))+') but only got a subset ('+str(len(images_received))+')')

        return req_dict


def write_to_tmpdir(req_dict, prefix='tesseract_recognize_api_tmp_', basedir='/tmp'):
    """Writes images and page xml from a request to a temporal directory.

    Args:
        req_dict (dict):     Parsed Page XML request.
        prefix (str):        Prefix for temporal directory name.
        basedir (str):       Base temporal directory.

    Returns:
        The path to the temporal directory where saved.
    """
    tmpdir = tempfile.mkdtemp(prefix=prefix, dir=basedir)
    if req_dict['pagexml'] is not None:
        fxml = os.path.basename(req_dict['pagexml']['filename'])
        with open(os.path.join(tmpdir, fxml), 'w') as f:
            f.write(req_dict['pagexml']['string'])
    if req_dict['images'] is not None:
        for image in req_dict['images']:
            image.save(os.path.join(tmpdir, os.path.basename(image.filename)))
    return tmpdir


class images_pagexml_request:
    """Decorator class for endpoints receiving images with optionally a page xml and responding with a page xml."""

    def __init__(self,
                 api,
                 images_help='Images with file names as referenced in the Page XML if given.',
                 pagexml_help='Optional valid Page XML file.',
                 options_help='Optional configuration options to be used for processing.',
                 response_help='Resulting Page XML after processing.'):
        """Initializer for images_pagexml_request class.

        Args:
            api (flask_restplus.Api): The flask_restplus Api instance.
            images_help (str):        Help for images field in swagger documentation.
            pagexml_help (str):       Help for pagexml field in swagger documentation.
            options_help (str):       Help for config field in swagger documentation.
            response_help (str):      Help for pagexml response in swagger documentation.
        """
        self.api = api
        self.response_help = response_help

        parser = ParserPageXML(bundle_errors=True)
        parser.add_argument('images',
            location='files',
            type=FileStorage,
            required=True,
            action='append',
            help=images_help)
        parser.add_argument('pagexml',
            location='files',
            type=TypePageXML,
            required=False,
            help=pagexml_help)
        parser.add_argument('options',
            location='form',
            type=str,
            required=False,
            default=[],
            action='append',
            help=options_help)
        self.parser = parser

    def __call__(self, method):
        """Makes a flask_restplus.Resource method expect a page xml and/or respond with a page xml."""
        method = self.api.expect(self.parser)(method)
        method = self.api.response(200, description=self.response_help)(method)
        method = self.api.produces(['application/xml'])(method)

        @wraps(method)
        def images_pagexml_request_wrapper(func):
            req_dict = self.parser.parse_args()
            pxml = method(func, req_dict)
            return Response(
                pxml.toString(True),
                mimetype='application/xml',
                headers={'Content-type': 'application/xml; charset=utf-8'})

        return images_pagexml_request_wrapper


def run_tesseract_recognize(*args):
    """Runs a tesseract-recognize command using given arguments."""
    cmd = ['tesseract-recognize']
    cmd.extend(list(args))

    proc = Popen(cmd, shell=False, stdin=PIPE, stdout=PIPE, stderr=STDOUT, close_fds=True)
    cmd_out = proc.stdout.read().decode("utf-8")
    proc.communicate()
    cmd_rc = proc.returncode

    return cmd_rc, cmd_out


if __name__ == '__main__':
    ## Parse config ##
    parser = get_cli_parser(logger=os.path.basename(__file__))
    cfg = parser.parse_args(env=True)

    ## Create a Flask WSGI application ##
    app = Flask(__name__)  # pylint: disable=invalid-name
    app.logger = parser.logger

    ## Create a Flask-RESTPlus API ##
    api = Api(app,
              doc=cfg.prefix+'/swagger',
              version='2.0',
              prefix=cfg.prefix,
              title='tesseract-recognize API',
              description='An API for running tesseract-recognition jobs.')
    sys.modules['flask.cli'].show_server_banner = lambda *x: None  # type: ignore


    ## Definition of endpoints ##
    @api.route('/openapi.json')
    class OpenAPI(Resource):
        def get(self):
            """Endpoint to get the OpenAPI json."""
            absurl = url.absurl(request.base_url.replace(request.path, cfg.prefix+'/swagger.json'))
            content, _ = convert_url(absurl)
            return json.loads(content)


    @api.route('/version')
    class ServiceVersion(Resource):
        @api.response(200, description='Version of the running service.')
        @api.produces(['text/plain'])
        def get(self):
            """Endpoint to get the version of the running service."""
            rc, out = run_tesseract_recognize('--version')
            if rc != 0:
                abort(500, 'problems getting version from tesseract-recognize command :: '+str(out))
            return Response(out, mimetype='text/plain')


    @api.route('/help')
    class ServiceHelp(Resource):
        @api.response(200, description='Help for the running service.')
        @api.produces(['text/plain'])
        def get(self):
            """Endpoint to get the help for the running service."""
            rc, out = run_tesseract_recognize('--help')
            if rc != 0:
                abort(500, 'problems getting help from tesseract-recognize command :: '+str(out))
            return Response(out, mimetype='text/plain')


    num_requests = 0
    @api.route('/process')
    class ProcessRequest(Resource):
        @images_pagexml_request(api)
        @api.doc(responses={400: 'tesseract-recognize execution failed.'})
        def post(self, req_dict):
            """Endpoint for running tesseract-recognize on given images or page xml file."""
            start_time = time()
            done_queue = queue.Queue()
            process_queue.put((done_queue, req_dict))
            while True:
                try:
                    thread, num_requests, pxml = done_queue.get(True, 0.05)
                    break
                except queue.Empty:
                    continue
            if isinstance(pxml, Exception):
                app.logger.error('Request '+str(num_requests)+' on thread '+str(thread)+' unsuccessful, '
                                 +('%.4g' % (time()-start_time))+' sec. :: '+str(pxml))
                abort(400, 'processing failed :: '+str(pxml))
            else:
                app.logger.info('Request '+str(num_requests)+' on thread '+str(thread)+' successful, '
                                +('%.4g' % (time()-start_time))+' sec.')
                return pxml


    process_queue = queue.Queue()  # type: ignore


    ## Processor thread function ##
    def start_processing(thread, process_queue):

        num_requests = 0
        tmpdir = None
        while True:
            try:
                done_queue, req_dict = process_queue.get(True, 0.05)
                num_requests += 1
                tmpdir = write_to_tmpdir(req_dict)

                opts = list(req_dict['options'])
                if len(opts) == 1 and opts[0][0] == '[':
                    opts = json.loads(opts[0])
                if req_dict['pagexml'] is not None:
                    opts.append(os.path.join(tmpdir, os.path.basename(req_dict['pagexml']['filename'])))
                elif req_dict['images'] is not None:
                    for image in req_dict['images']:
                        opts.append(os.path.join(tmpdir, os.path.basename(image.filename)))
                else:
                    raise KeyError('No images found in request.')
                opts.extend(['-o', os.path.join(tmpdir, 'output.xml')])

                rc, out = run_tesseract_recognize(*opts)
                if rc != 0:
                    raise RuntimeError('tesseract-recognize execution failed :: opts: '+str(opts)+' :: '+str(out))

                pxml = pagexml.PageXML(os.path.join(tmpdir, 'output.xml'))
                done_queue.put((thread, num_requests, pxml))

            except queue.Empty:
                continue
            except json.decoder.JSONDecodeError as ex:
                done_queue.put((thread, num_requests, RuntimeError('JSONDecodeError: '+str(ex)+' while parsing '+opts[0])))
            except Exception as ex:
                done_queue.put((thread, num_requests, ex))
            finally:
                if not cfg.debug and tmpdir is not None:
                    shutil.rmtree(tmpdir)
                    tmpdir = None


    for thread in range(cfg.threads):
        threading.Thread(target=start_processing, args=(thread+1, process_queue)).start()


    app.run(host=cfg.host, port=cfg.port, debug=cfg.debug)
