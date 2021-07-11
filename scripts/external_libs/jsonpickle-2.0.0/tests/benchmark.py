#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright (C) 2008 John Paulett (john -at- paulett.org)
# All rights reserved.
#
# This software is licensed as described in the file COPYING, which
# you should have received as part of this distribution.

import argparse
import os
import sys
import timeit


json = """\
import feedparser
import jsonpickle
import feedparser_test as test
doc = feedparser.parse(test.RSS_DOC)

jsonpickle.set_preferred_backend('%s')

pickled = jsonpickle.encode(doc)
unpickled = jsonpickle.decode(pickled)
if doc['feed']['title'] != unpickled['feed']['title']:
    print 'Not a match'
"""


if __name__ == '__main__':
    sys.path.insert(1, os.getcwd())

    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-n',
        '--number',
        type=int,
        default=500,
        help='number of iterations to benchmark',
    )
    parser.add_argument('mod', nargs='?', default='json', help='json module')

    args = parser.parse_args()
    mod = args.mod
    number = args.number

    print('Using %s' % mod)
    json_test = timeit.Timer(stmt=json % mod)
    print("%.9f sec/pass " % (json_test.timeit(number=number) / number))
