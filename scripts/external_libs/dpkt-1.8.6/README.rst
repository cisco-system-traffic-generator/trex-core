
====
dpkt
====

| |docs| |travis| |coveralls| |landscape| |version|
| |downloads| |wheel| |supported-versions| |supported-implementations|

.. |docs| image:: https://readthedocs.org/projects/dpkt/badge/?style=flat
    :target: https://readthedocs.org/projects/dpkt
    :alt: Documentation Status

.. |travis| image:: http://img.shields.io/travis/kbandla/dpkt/master.png?style=flat
    :alt: Travis-CI Build Status
    :target: https://travis-ci.org/kbandla/dpkt

.. |coveralls| image:: http://img.shields.io/coveralls/kbandla/dpkt/master.png?style=flat
    :alt: Coverage Status
    :target: https://coveralls.io/r/kbandla/dpkt

.. |landscape| image:: https://landscape.io/github/kbandla/dpkt/master/landscape.svg?style=flat
    :target: https://landscape.io/github/kbandla/dpkt/master
    :alt: Code Quality Status

.. |version| image:: http://img.shields.io/pypi/v/dpkt.png?style=flat
    :alt: PyPI Package latest release
    :target: https://pypi.python.org/pypi/dpkt

.. |downloads| image:: http://img.shields.io/pypi/dm/dpkt.png?style=flat
    :alt: PyPI Package monthly downloads
    :target: https://pypi.python.org/pypi/dpkt

.. |wheel| image:: https://pypip.in/wheel/dpkt/badge.png?style=flat
    :alt: PyPI Wheel
    :target: https://pypi.python.org/pypi/dpkt

.. |supported-versions| image:: https://pypip.in/py_versions/dpkt/badge.png?style=flat
    :alt: Supported versions
    :target: https://pypi.python.org/pypi/dpkt

.. |supported-implementations| image:: https://pypip.in/implementation/dpkt/badge.png?style=flat
    :alt: Supported implementations
    :target: https://pypi.python.org/pypi/dpkt

Installation
============

::

    pip install dpkt

Documentation
=============

https://dpkt.readthedocs.org/

Development
===========

To run the all tests run::

    tox


Deviations from upstream
~~~~~~~~~~~~~~~~~~~~~~~~

This code is based on `dpkt code <https://code.google.com/p/dpkt/>`__ lead by Dug Song.

At this point, this is not the exact `upstream
version <https://code.google.com/p/dpkt/>`__. If you are looking for the
latest stock dpkt, please get it from the above link.

Almost all of the upstream changes are pulled. However, some modules are
not. Here is a list of the changes:

-  `dpkt/dpkt.py <https://github.com/kbandla/dpkt/commit/336fe02b0e2f00b382d91cd42558a69eec16d6c7>`__:
   decouple dnet from dpkt
-  `dpkt/dns.py <https://github.com/kbandla/dpkt/commit/2bf3cde213144391fd90488d12f9ccce51b5fbca>`__
   : parse some more DNS flags

Examples
--------

[@jonoberheide's](https://twitter.com/jonoberheide) old examples still
apply:

-  `dpkt Tutorial #1: ICMP
   Echo <https://jon.oberheide.org/blog/2008/08/25/dpkt-tutorial-1-icmp-echo/>`__
-  `dpkt Tutorial #2: Parsing a PCAP
   File <https://jon.oberheide.org/blog/2008/10/15/dpkt-tutorial-2-parsing-a-pcap-file/>`__
-  `dpkt Tutorial #3: dns
   spoofing <https://jon.oberheide.org/blog/2008/12/20/dpkt-tutorial-3-dns-spoofing/>`__
-  `dpkt Tutorial #4: AS Paths from
   MRT/BGP <https://jon.oberheide.org/blog/2009/03/25/dpkt-tutorial-4-as-paths-from-mrt-bgp/>`__

`Jeff Silverman <https://github.com/jeffsilverm>`__ has some
`code <https://github.com/jeffsilverm/dpkt_doc>`__ and
`documentation <http://www.commercialventvac.com/dpkt.html>`__.

LICENSE
-------

BSD 3-Clause License, as the upstream project
