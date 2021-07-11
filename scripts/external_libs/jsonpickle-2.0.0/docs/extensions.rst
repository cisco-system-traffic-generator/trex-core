=====================
jsonpickle extensions
=====================

NumPy
-----
jsonpickle includes a built-in numpy extension.  If would like to encode
sklearn models, numpy arrays, and other numpy-based data then you must
enable the numpy extension by registering its handlers::

    >>> import jsonpickle.ext.numpy as jsonpickle_numpy
    >>> jsonpickle_numpy.register_handlers()
