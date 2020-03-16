#!/usr/bin/env python3
# coding=utf-8
#
# File: server.py
# Author: Hankso
# Webpage: https://github.com/hankso
# Time: Sun 02 Jun 2019 00:46:30 CST

'''File Manager implemented in Python, for debug only.'''

import os
import json
import random
import bottle

__dir__ = os.path.dirname(os.path.abspath(__file__))
__src__ = os.path.join(__dir__, 'data', 'src')


@bottle.get('/')
def get_root():
    bottle.redirect('index.html')


@bottle.get('/list')
def get_list():
    path = os.path.join(
        __src__, bottle.request.query.get('dir', '').strip('/'))
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
            'time': int(os.path.getmtime(fpath))
        })
    bottle.response.content_type = 'application/json'
    return json.dumps(d)


@bottle.get('/temp')
def get_temp():
    return json.dumps([random.randint(50, 100) for i in range(6)])


@bottle.get('/<filename:path>')
def static(filename):
    if os.path.exists(os.path.join(__src__, filename)):
        return bottle.static_file(filename, __src__)
    else:
        return bottle.static_file(filename + '.gz', __src__)


if __name__ == '__main__':
    bottle.run(reload=True)
