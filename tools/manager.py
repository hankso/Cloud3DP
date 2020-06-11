#!/usr/bin/env python3
# coding=utf-8
#
# File: manager.py
# Author: Hankso
# Webpage: https://github.com/hankso
# Time: Sun 02 Jun 2019 00:46:30 CST

# built-in
import os
import re
import sys
import gzip
import glob
import json
import argparse

# these are default values
__basedir__ = os.path.dirname(os.path.abspath(__file__))
__distdir__ = os.path.join(__basedir__, '..', 'webdev', 'dist')
__nvsfile__ = os.path.join(__basedir__, '..', 'nvs_flash.csv')


def _random_id(len=8):
    #  from random import choice
    #  from string import hexdigits
    #  return ''.join([choice(hexdigits) for i in range(len)])
    from uuid import uuid4
    return uuid4().hex[:len].upper()


def genid(args):
    out = open(args.out, 'w') if type(args.out) is str else args.out
    data = _random_id(args.len)
    if os.path.exists(args.tpl):
        with open(args.tpl, 'r') as f:
            data = f.read().replace('{UID}', data)
    out.write(data)
    if type(args.out) is str:
        out.close()


def _relpath(p, ref='.', strip=True):
    p = os.path.relpath(p, ref)
    if not strip:
        return p
    if (len(p.split(os.path.sep)) > 3 and len(p) > 30) or len(p) > 80:
        m = re.findall(r'\w+%s(.+)%s\w+' % tuple([os.path.sep] * 2), p)
        p = p.replace((m or [''])[0], '...')
    return p


def _absjoin(*a):
    return os.path.abspath(os.path.join(*a))


def webserver(args):
    # requirements.txt: bottle
    import bottle

    args.root = os.path.abspath(args.root)
    if not (os.path.exists(args.root)):
        return print('Cannot serve at `%s`: dirctory not found' % args.root)
    print('WebServer running at `%s`' % _relpath(args.root))

    def edit_get():
        if bottle.request.query.get('list'):
            name = bottle.request.query.get('list').strip('/')
            path = os.path.join(args.root, name)
            if not os.path.exists(path):
                bottle.abort(404)
            elif os.path.isfile(path):
                bottle.abort(403)
            d = []
            for fn in os.listdir(path):
                fpath = os.path.join(path, fn)
                d.append({
                    'name': fn,
                    'type': 'folder' if os.path.isdir(fpath) else 'file',
                    'size': os.path.getsize(fpath),
                    'date': int(os.path.getmtime(fpath)) * 1000
                })
            bottle.response.content_type = 'application/json'
            return json.dumps(d)
        elif bottle.request.query.get('path'):
            return bottle.static_file(
                bottle.request.query.get('path').strip('/'), args.root,
                download=bool(bottle.request.query.get('download')))
        else:
            return bottle.redirect('/ap/editor.html')

    def edit_create():
        print(dict(bottle.request.params.items()))

    def edit_delete():
        print(dict(bottle.request.params.items()))

    def edit_upload():
        print(dict(bottle.request.POST.items()))

    def config():
        if bottle.request.method == 'GET':
            return {
                'a.b.c': 123,       # number
                'a.b.d': 'adsf',    # string
                'e.f.g': True,      # boolean
                'e.h.i': '1',       # fake boolean
                'x.y.z': [1, 2]     # invalid type
            }
        else:
            print(dict(bottle.request.params.items()))

    def fileman():
        if not hasattr(fileman, 'tpl'):
            p1 = _absjoin(args.root, '**', 'fileman*', 'index*.html*')
            p2 = _absjoin(args.root, '**', 'fileman*.html*')
            tpls = (glob.glob(p1, recursive=True) +
                    glob.glob(p2, recursive=True))
            if tpls:
                print('Using template %s' % _relpath(tpls[0]))
                with open(tpls[0], 'rb') as f:
                    bstr = f.read()
                if tpls[0].endswith('.gz'):
                    bstr = gzip.decompress(bstr)
                fileman.tpl = bstr.decode('utf-8')
            else:
                fileman.tpl = None
        if not fileman.tpl:
            return bottle.HTTPError(404, 'Cannot open directory')
        return fileman.tpl.replace('%ROOT%', bottle.request.path)

    def static(filename, man=True, auto=True, redirect=False, root=args.root):
        '''
        1. auto append ".gz" if resolving file
        2. auto detect "index.html" if resolving directory
        3. fallback to file manager if no "index.html" under dir
        '''
        fn = os.path.join(root, filename.strip('/\\'))
        if os.path.exists(fn):
            if os.path.isfile(fn):
                return bottle.static_file(filename, root)
            elif not os.path.isdir(fn):
                return bottle.HTTPError(404, 'Path not found')
            if auto and os.path.exists(os.path.join(fn, 'index.html')):
                if redirect:
                    fn = os.path.join('/', filename, 'index.html')
                    return bottle.redirect(fn)
                else:
                    fn = os.path.join(filename, 'index.html')
                    return bottle.static_file(fn, args.root)
            if not man:
                return bottle.HTTPError(404, 'Cannot serve directory')
            return fileman()
        elif os.path.exists(fn + '.gz'):
            return bottle.static_file(filename + '.gz', root)
        return bottle.HTTPError(404, 'File not found')

    def static_assets(filename):
        srcdir = os.path.join(args.root, 'src')
        if os.path.exists(srcdir):
            filename = os.path.join('src', filename)
        else:
            filename = bottle.request.path  # treat as normal static file
        return static(filename, man=False, auto=False, redirect=False)

    def static_files(filename='/'):
        auto = bottle.request.query.get('auto', True)
        return static(filename, not args.static, auto)

    app = bottle.Bottle()
    if not args.static:
        print('Will simulate ESP32 APIs: edit/config/assets etc.')
        app.route('/edit', 'GET', edit_get)
        app.route('/editu', 'POST', edit_upload)
        app.route('/editc', ['GET', 'POST', 'PUT'], edit_create)
        app.route('/editd', ['GET', 'POST', 'DELETE'], edit_delete)
        app.route('/config', ['GET', 'POST'], config)
        app.route('/assets/<filename:path>', 'GET', static_assets)
    app.route('/', 'GET', static_files)
    app.route('/<filename:path>', 'GET', static_files)
    bottle.run(app, reload=True, host=args.host, port=args.port)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(epilog='see <command> -h for more')
    parser.set_defaults(func=lambda args: parser.print_help())
    subparsers = parser.add_subparsers(
        prog='', title='Supported Commands are', metavar='')

    sparser = subparsers.add_parser(
        'serve', help='Python implemented WebServer to debug (bottle needed)')
    sparser.add_argument(
        '-H', '--host', default='0.0.0.0', help='Host address to listen on')
    sparser.add_argument(
        '-P', '--port', type=int, default=8080, help='default port 8080')
    sparser.add_argument(
        '--static', action='count', help='disable ESP32 API, only statics')
    sparser.add_argument(
        'root', nargs='?', default=__distdir__, help='path to static files')
    sparser.set_defaults(func=webserver)

    sparser = subparsers.add_parser(
        'genid', help='Generate unique ID in NVS flash for each firmware')
    sparser.add_argument(
        '-l', '--len', type=int, default=6, help='length of UID')
    sparser.add_argument(
        '-o', '--out', default=sys.stdout, help='write to file if specified')
    sparser.add_argument(
        '-f', '--tpl', default=__nvsfile__, help='render nvs from template')
    sparser.set_defaults(func=genid)

    args = parser.parse_args(sys.argv[1:])
    sys.exit(args.func(args))
