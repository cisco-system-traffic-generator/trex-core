#!/usr/bin/env python
#
# texttable - module for creating simple ASCII tables
# Copyright (C) 2003-2015 Gerome Fournier <jef(at)foutaise.org>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

DESCRIPTION = "module for creating simple ASCII tables"

LONG_DESCRIPTION = """\
texttable is a module to generate a formatted text table, using ASCII
characters."""

import sys

from distutils.core import setup
if sys.version < '2.2.3':
    from distutils.dist import DistributionMetadata
    DistributionMetadata.classifiers = None
    DistributionMetadata.download_url = None

setup(
    name = "texttable",
    version = "0.8.4",
    author = "Gerome Fournier", 
    author_email = "jef(at)foutaise.org",
    url = "https://github.com/foutaise/texttable/",
    download_url = "https://github.com/foutaise/texttable/archive/v0.8.4.tar.gz",
    license = "LGPL",
    py_modules = ["texttable"],
    description = DESCRIPTION,
    long_description = LONG_DESCRIPTION,
    platforms = "any",
    classifiers = [
        'Development Status :: 4 - Beta',
        'Environment :: Console',
        'Intended Audience :: Developers',
        'Intended Audience :: End Users/Desktop',
        'License :: OSI Approved :: GNU Library or Lesser General Public License (LGPL)',
        'Operating System :: Microsoft :: Windows',
        'Operating System :: POSIX',
        'Operating System :: MacOS',
        'Topic :: Software Development :: Libraries :: Python Modules',
        'Topic :: Text Processing',
        'Topic :: Utilities',
    ]
)
