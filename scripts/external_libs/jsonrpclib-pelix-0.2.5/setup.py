#!/usr/bin/env python
# -- Content-Encoding: UTF-8 --
"""
Installation script

:authors: Josh Marshall, Thomas Calmant
:copyright: Copyright 2015, isandlaTech
:license: Apache License 2.0
:version: 0.2.5

..

    Copyright 2015 isandlaTech

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
"""

# Module version
__version_info__ = (0, 2, 5)
__version__ = ".".join(str(x) for x in __version_info__)

# Documentation strings format
__docformat__ = "restructuredtext en"

# ------------------------------------------------------------------------------

import sys

try:
    from setuptools import setup
except ImportError:
    from distutils.core import setup

# ------------------------------------------------------------------------------

setup(
    name="jsonrpclib-pelix",
    version=__version__,
    license="Apache License 2.0",
    author="Thomas Calmant",
    author_email="thomas.calmant+github@gmail.com",
    url="http://github.com/tcalmant/jsonrpclib/",
    description=
    "This project is an implementation of the JSON-RPC v2.0 specification "
    "(backwards-compatible) as a client library, for Python 2.6+ and Python 3."
    "This version is a fork of jsonrpclib by Josh Marshall, "
    "usable with Pelix remote services.",
    long_description=open("README.rst").read(),
    packages=["jsonrpclib"],
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: Apache Software License',
        'Operating System :: OS Independent',
        'Programming Language :: Python :: 2.6',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.0',
        'Programming Language :: Python :: 3.1',
        'Programming Language :: Python :: 3.2',
        'Programming Language :: Python :: 3.3',
        'Programming Language :: Python :: 3.4'],
    tests_require=['unittest2'] if sys.version_info < (2, 7) else []
)
