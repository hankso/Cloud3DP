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
import glob
import json
import uuid
import argparse

__basedir__ = os.path.dirname(os.path.abspath(__file__))


def random_id(args):
    out = open(args.out, 'w') if type(args.out) is str else args.out
    #  from random import choice
    #  from string import hexdigits
    #  out.write(''.join([choice(hexdigits) for i in range(args.len)]))
    out.write(uuid.uuid4().hex[:args.len].upper())
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

    if not args.root:
        args.root = os.path.join(__basedir__, 'dist')
    args.root = os.path.abspath(args.root)
    if not (os.path.exists(args.root)):
        return print('Cannot serve at `%s`: dirctory not found' % args.root)
    print('WebServer running at `%s`' % _relpath(args.root))

    @bottle.get('/edit')
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

    @bottle.route('/editc', ['GET', 'POST', 'PUT'])
    def edit_create():
        print(dict(bottle.request.params.items()))

    @bottle.route('/editd', ['GET', 'POST', 'DELETE'])
    def edit_delete():
        print(dict(bottle.request.params.items()))

    @bottle.post('/editu')
    def edit_upload():
        print(dict(bottle.request.POST.items()))

    @bottle.route('/config', ['GET', 'POST'])
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
        if not fileman.files:
            return bottle.HTTPError(404, 'Cannot open directory')
        with open(fileman.files[0], 'r') as f:
            return f.read().replace('%ROOT%', bottle.request.path)
    pattern1 = _absjoin(args.root, '**', 'fileman*', 'index*.html')
    pattern2 = _absjoin(args.root, '**', 'fileman*.html')
    fileman.files = (
        glob.glob(pattern1, recursive=True) +
        glob.glob(pattern2, recursive=True))
    if fileman.files:
        print('Using template %s' % _relpath(fileman.files[0]))

    def static(filename, auto=True, root=args.root):
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
                return bottle.HTTPError(404, 'File not found')
            if auto and os.path.exists(os.path.join(fn, 'index.html')):
                index = os.path.join('/', filename, 'index.html')
                return bottle.redirect(index)
            return fileman()
        elif os.path.exists(fn + '.gz'):
            return bottle.static_file(filename + '.gz', root)
        return bottle.HTTPError(404, 'File not found')

    @bottle.route('/assets/<filename:path>')
    def assets(filename):
        srcdir = os.path.join(args.root, 'src')
        if os.path.exists(srcdir):
            filename = os.path.join('src', filename)
        else:
            filename = bottle.request.path  # treat as normal static file
        return static(filename, False)

    @bottle.get('/')
    @bottle.get('/<filename:path>')
    def root(filename='/'):
        return static(filename, bottle.request.query.get('auto', True))

    bottle.run(reload=True, host=args.host, port=args.port)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(epilog='see <command> -h for more')
    parser.set_defaults(func=lambda args: parser.print_help())
    subparsers = parser.add_subparsers(
        prog='', title='Supported Commands are', metavar='')

    sparser = subparsers.add_parser(
        'serve', help='Python implemented WebServer to debug (bottle needed)')
    sparser.add_argument('-H', '--host', type=str, default='0.0.0.0')
    sparser.add_argument('-P', '--port', type=int, default=8080)
    sparser.add_argument('root', nargs='?')
    sparser.set_defaults(func=webserver)

    sparser = subparsers.add_parser(
        'genid', help='Generate unique ID in NVS flash for each chip')
    sparser.add_argument('-l', '--len', type=int, default=6)
    sparser.add_argument('-o', '--out', default=sys.stdout)
    sparser.set_defaults(func=random_id)

    args = parser.parse_args(sys.argv[1:])
    sys.exit(args.func(args))
