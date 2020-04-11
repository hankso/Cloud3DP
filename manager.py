#!/usr/bin/env python3
# coding=utf-8
#
# File: manager.py
# Author: Hankso
# Webpage: https://github.com/hankso
# Time: Sun 02 Jun 2019 00:46:30 CST

# built-in
import os
import sys
import time
import json
import uuid
import argparse

__basedir__ = os.path.dirname(os.path.abspath(__file__))


def random_id(args):
    out = open(args.out, 'w') if type(args.out) is str else args.out
    #  from random import choice
    #  from string import hexdigits
    #  out.write(''.join([choice(hexdigits) for i in range(args.len)]))
    out.write(uuid.uuid4().hex[:args.len])
    if type(args.out) is str:
        out.close()


def _absjoin(*a):
    return os.path.abspath(os.path.join(*a))


def _mtime(fn):
    return os.stat(fn).st_mtime


def copy_files(args):
    import io
    import gzip
    import shutil
    inpfn = _absjoin(__basedir__, args.input)
    with open(inpfn, 'r') as f:
        obj = json.load(f)
    inppath = os.path.dirname(inpfn)
    srcdir = _absjoin(inppath, obj['srcdir'])
    dstdir = _absjoin(inppath, obj['dstdir'])

    for path in obj['dirs']:
        path = _absjoin(dstdir, path)
        if not os.path.exists(path):
            os.makedirs(path)

    inpmtime = _mtime(inpfn)
    lasttime = obj.get('mtime', {'this': inpmtime})
    for src, dst, compress in obj['files']:
        srcfn = _absjoin(srcdir, src)
        dstfn = _absjoin(dstdir, dst)
        try:
            if (
                not args.force and
                src in lasttime and
                (inpmtime - lasttime['this']) < 0.5 and
                _mtime(srcfn) <= lasttime[src]
            ):
                print('Skip file {}: not changed'.format(src))
                continue
            else:
                lasttime[src] = _mtime(srcfn)
            if compress:
                dst += '.gz'
                dstfn += '.gz'
                with open(srcfn, 'rb') as fsrc, open(dstfn, 'wb') as fdst:
                    buf = io.BytesIO()
                    with gzip.GzipFile(None, 'wb', 9, fileobj=buf) as fgzip:
                        fgzip.write(fsrc.read())
                    fdst.write(buf.getvalue())
            else:
                shutil.copy(srcfn, dstfn)
            print('Move file {} -> {}'.format(src, dst))
        except Exception as e:
            print('Moving {} failed: {}'.format(src, e))

    lasttime['this'] = time.time()
    obj['mtime'] = lasttime
    with open(inpfn, 'w') as f:
        json.dump(obj, f, indent=2, separators=(',', ': '))


def webserver(args):
    # requirements.txt: bottle
    import bottle
    hostdir = os.path.join(__basedir__, 'dist')

    @bottle.get('/')
    def root():
        bottle.redirect('index.html')

    @bottle.get('/edit')
    def edit_get():
        if bottle.request.query.get('list'):
            name = bottle.request.query.get('list').strip('/')
            path = os.path.join(hostdir, name)
            if not os.path.exists(path):
                bottle.error(404)
            elif os.path.isfile(path):
                bottle.error(403)
            d = []
            for fn in os.listdir(path):
                fpath = os.path.join(path, fn)
                d.append({
                    'name': fn,
                    'type': 'dir' if os.path.isdir(fpath) else 'file',
                    'size': os.path.getsize(fpath),
                    'date': int(os.path.getmtime(fpath)) * 1000
                })
            bottle.response.content_type = 'application/json'
            return json.dumps(d)
        elif bottle.request.query.get('path'):
            return bottle.static_file(
                bottle.request.query.get('path').strip('/'), hostdir,
                download=bool(bottle.request.query.get('download')))
        else:
            return bottle.redirect('/ap/editor.html')

    @bottle.route('/editc', ["GET", "POST", "PUT"])
    def edit_create():
        for k, v in bottle.request.params.items():
            print(k, v)

    @bottle.route('/editd', ["GET", "POST", "DELETE"])
    def edit_delete():
        for k, v in bottle.request.params.items():
            print(k, v)

    @bottle.post('/editu')
    def edit_upload():
        for k, v in bottle.request.POST.items():
            print(k, v)

    @bottle.get('/<filename:path>')
    def static(filename):
        fn = os.path.join(hostdir, filename)
        if os.path.exists(fn):
            return bottle.static_file(filename, hostdir)
        elif os.path.exists(fn + ".gz"):
            return bottle.static_file(filename + ".gz", hostdir)
        return bottle.HTTPError(404, "File not found")

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
    sparser.set_defaults(func=webserver)

    sparser = subparsers.add_parser(
        'genid', help='Generate unique ID in NVS flash for each chip')
    sparser.add_argument('-l', '--len', type=int, default=8)
    sparser.add_argument('-o', '--out', type=str, default=sys.stdout)
    sparser.set_defaults(func=random_id)

    sparser = subparsers.add_parser(
        'move', help='Copy web files from source to dist directory')
    sparser.add_argument(
        '-i', '--input', type=str, help='JSON file contains SPIFFS info')
    sparser.add_argument('-f', '--force', action='count')
    sparser.set_defaults(func=copy_files, input='websrc/build-files.json')

    args = parser.parse_args(sys.argv[1:])
    sys.exit(args.func(args))
