========================
jsonpickle Documentation
========================

``jsonpickle`` is a Python library for
serialization and deserialization of complex Python objects to and from
JSON.  The standard Python libraries for encoding Python into JSON, such as
the stdlib's json, simplejson, and demjson, can only handle Python
primitives that have a direct JSON equivalent (e.g. dicts, lists, strings,
ints, etc.).  jsonpickle builds on top of these libraries and allows more
complex data structures to be serialized to JSON. jsonpickle is highly
configurable and extendable--allowing the user to choose the JSON backend
and add additional backends.

.. contents::

jsonpickle Usage
================

.. note::

   Please see the note in the :ref:`api-docs` when serializing dictionaries
   that contain non-string dictionary keys.

.. automodule:: jsonpickle

Download & Install
==================

The easiest way to get jsonpickle is via PyPi_ with pip_::

    $ pip install -U jsonpickle

jsonpickle has no required dependencies (it uses the standard library's
:mod:`json` module by default).

You can also download or :ref:`checkout <jsonpickle-contrib-checkout>` the
latest code and install from source::

    $ python setup.py install

.. _PyPi: https://pypi.org/project/jsonpickle/
.. _pip: https://pypi.org/project/pip/
.. _download: https://pypi.org/project/jsonpickle/


API Reference
=============

.. toctree::

   api

Extensions
==========

.. toctree::

   extensions

Contributing
============

.. toctree::

   contrib

Contact
=======

Please join our `mailing list <https://groups.google.com/group/jsonpickle>`_.
You can send email to *jsonpickle@googlegroups.com*.

Check https://github.com/jsonpickle/jsonpickle for project updates.


Authors
=======

 * John Paulett - john -at- paulett.org - https://github.com/johnpaulett
 * David Aguilar - davvid -at- gmail.com - https://github.com/davvid
 * Dan Buch - https://github.com/meatballhat
 * Ian Schenck - https://github.com/ianschenck
 * David K. Hess - https://github.com/davidkhess
 * Alec Thomas - https://github.com/alecthomas
 * jaraco - https://github.com/jaraco
 * Marcin Tustin - https://github.com/marcintustin


Change Log
==========

.. toctree::
   :maxdepth: 2

   history

License
=======

jsonpickle is provided under a
`New BSD license <https://github.com/jsonpickle/jsonpickle/raw/master/COPYING>`_,

Copyright (C) 2008-2011 John Paulett (john -at- paulett.org)
Copyright (C) 2009-2016 David Aguilar (davvid -at- gmail.com)
