jsonpickleJS
============

Javascript reinterpretation of Python jsonpickle to allow reading and (to a lesser extent) 
writing JSON objects

Copyright (c) 2014 Michael Scott Cuthbert and cuthbertLab.
Released under the BSD (3-clause) license. See LICENSE.

Python to Javascript and Back
==============================
Python has a remarkable number of ways (for a language that believes there's only one way to do it)
to transfer data between itself and Javascript, most obviously with the 
``json.dump()``/``json.dumps()`` calls, which work well on specifically created data, especially
of primitive objects. It also has a good number of ways to store the contents of an object so that
it could be reloaded later into the same Python interpreter/session. The most well known of these
is the ``pickle`` module, which stores the Python data as binary data.

The external ``jsonpickle`` module, which you'll need to have installed to make this module have
any sense, combines the features of ``json`` and ``pickle``, storing Python data in a JSON 
(Javascript Object Notation) string, via ``jsonpickle.encode()`` that can be parsed back by 
``jsonpickle.decode()`` to get nearly all data types restored to their previous state. (Highly
recommended: jsonpickle v1.7 or higher.  Older versions will not serialize recursive
data structures as necessary for a lot of applications.)

Since JSON is one of the most easily parsed formats in Javascript (the name should be a giveaway),
this project, jsonpickleJS, exists to parse the output of Python's jsonpickle into objects in
Javascript.  The constructors of the objects need to have the same names and exist in the global
namespace, such as ``window``, in the Javascript.  For instance, if you have a class called
``myobject.Thing`` in Python, you'll need to have a Prototype constructor function called 
``window.myobject.Thing`` in Javascript. The object, and any subobjects, will be created as closely
as possible in Javascript.

The reverse is also possible, with some caveats. Since Javascript doesn't (until ECMAScript 6) have
the concept of named classes, each object will need to have a marker somewhere on it saying what
Python object it should convert back to. The marker is 
``o[jsonpickle.tags.PY_CLASS] = 'fully.qualified.ClassName'``. 
It may be possible in the future to use ``instanceof``
through the entire Global namespace to figure out what something is, but that seems rather dangerous
and inefficient (A project for later). 

Limitations
===========
Remember that Javascript does not have tuples, so all tuple objects are changed to lists.
Namedtuples behave the same way, I believe. Dicts and Objects are identical in Javascript (both
a blessing and a curse).

Security
========
Pickle, jsonpickle, and jsonpickleJS all raise important security considerations you must be
aware of. You will be loading data directly into Python or Javascript with no checks on what the
data contains. Only load data you have personally produced if you want to be safe.  In Javascript,
malicious data may compromise your browser, browser history, functioning of the current web page,
etc. That's pretty bad, but nothing compared to what can happen if you load jsonpickle data
from Javascript into Python: a maliciously written set of jsonpickle data may be able to send the
contents of any file back to the net or manipulate or delete the hard drive of the server. It may
be that the parsed Python object have to be called in some way, but it may even be possible to have
malicious code executed just on parsing; assume the worst. Your .html/.js may only produce safe
data, but anyone with JS programming experience can inject other data into your server scripts. 
Be safe: be cautious going from Python to Javascript and NEVER accept Javascript-produced
jsonpickle data from the net into your Python program in any sort of open environment.

Usage
=====
See the source code of: testUnpickle.html to see how to use it. JsonpickleJS follows the AMD
moduleloader standard, so set the ``src=""`` attribute in the ``<script>`` to an AMD loader
such as ``//cdnjs.cloudflare.com/ajax/libs/require.js/2.1.14/require.min.js`` 
(or the included local version in ``jsonpickleJS/ext/require/require.js``) and 
the ``data-main`` attribute to ``jsonpickleJS/main`` (no ``.js``).  Then call 
``var o = jsonpickle.decode(jsonStr)`` to get the Python object back as a JS object named ``o``.

See the cuthbertLab/music21 and cuthbertLab/music21j projects and especially the ``.show('vexflow')``
component for an example of how jsonpickleJS can be extremely useful for projects that have
parallel data structures between Python and Javascript.