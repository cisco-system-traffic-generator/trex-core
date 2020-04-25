#!/usr/bin/env python
# -- Content-Encoding: UTF-8 --
"""
Installation script

:authors: Josh Marshall, Thomas Calmant
:copyright: Copyright 2020, Thomas Calmant
:license: Apache License 2.0
:version: 0.4.1

..

    Copyright 2020 Thomas Calmant

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
__version_info__ = (0, 4, 1)
__version__ = ".".join(str(x) for x in __version_info__)

# Documentation strings format
__docformat__ = "restructuredtext en"

# ------------------------------------------------------------------------------

try:
    from setuptools import setup
except ImportError:
    from distutils.core import setup

# ------------------------------------------------------------------------------

# Read the description file
with open("README.md", "r") as fh:
    long_description = fh.read()

setup(
    name="jsonrpclib-pelix",
    version=__version__,
    license="Apache License 2.0",
    author="Thomas Calmant",
    author_email="thomas.calmant+github@gmail.com",
    url="http://github.com/tcalmant/jsonrpclib/",
    description="This project is an implementation of the JSON-RPC v2.0 "
    "specification (backwards-compatible) as a client library, for Python 2.7 "
    "and Python 3. This version is a fork of jsonrpclib by Josh Marshall, "
    "made to be also usable with Pelix/iPOPO remote services.",
    long_description=long_description,
    long_description_content_type="text/markdown",
    packages=["jsonrpclib"],
    test_suite="tests",
    classifiers=[
        "Development Status :: 5 - Production/Stable",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: Apache Software License",
        "Operating System :: OS Independent",
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.3",
        "Programming Language :: Python :: 3.4",
        "Programming Language :: Python :: 3.5",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
    ],
)
