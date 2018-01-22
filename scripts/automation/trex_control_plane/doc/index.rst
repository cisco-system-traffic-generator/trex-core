.. TRex Control Plane documentation master file, created by
   sphinx-quickstart on Tue Jun  2 07:48:10 2015.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Welcome to TRex Control Plane's documentation!
==============================================

TRex is a **realistic traffic generator** that enables you to do get learn more about your under development devices.

This site covers the Python API of TRex control plane, and explains how to utilize it to your needs.
However, since the entire API is JSON-RPC [#f1]_ based, you may want to check out other implementations that could suit you.


To understand the entirely how the API works and how to set up the server side, check out the `trex-core Wiki <https://github.com/cisco-system-traffic-generator/trex-core/wiki>`_ under the documentation section of TRex website.
   
   
**Use the table of contents below or the menu to your left to navigate through the site**


Client Package
==============

| Starting from version v1.99 TRex has separated client package included in main directory.
| Put it at any place you like, preferably same place as your scripts.
| (If it's not at same place as your scripts, you will need to ensure trex_client directory is in sys.path)

Un-pack it using command::

 tar -xzf trex_client_<TRex version>.tar.gz

The client assumes stateful daemon is running::

 sudo ./trex_daemon_server start

After un-tarring the client package, you can verify basic tests in examples directory out of the box:

.. code-block:: bash

 cd trex_client/stf/examples
 python stf_example.py -s <server address>

You can verify that the traffic was sent and arrives properly if you see something like this::

 Connecting to 127.0.0.1
 Connected, start TRex
 Sample until end
 Test results:
 Is valid history?       True
 Done warmup?            True
 Expected tx rate:       {u'm_tx_expected_pps': 71898.4, u'm_tx_expected_bps': 535157280.0, u'm_tx_expected_cps': 1943.2}
 Current tx rate:        {u'm_tx_bps': 542338368.0, u'm_tx_cps': 1945.4, u'm_tx_pps': 79993.4}
 Maximum latency:        {u'max-4': 55, u'max-5': 30, u'max-6': 50, u'max-7': 30, u'max-0': 55, u'max-1': 40, u'max-2': 55, u'max-3': 30}
 Average latency:        {'all': 32.913, u'port6': 35.9, u'port7': 30.0, u'port4': 35.8, u'port5': 30.0, u'port2': 35.8, u'port3': 30.0, u'port0': 35.8, u'port1': 30.0}
 Average window latency: {'all': 31.543, u'port6': 32.871, u'port7': 28.929, u'port4': 33.886, u'port5': 28.929, u'port2': 33.843, u'port3': 28.929, u'port0': 33.871, u'port1': 31.086}
 Total drops:            -3
 Drop rate:              0.0
 History size so far:    7

 TX by ports:
 0: 230579  |  1: 359435  |  2: 230578  |  3: 359430  |  4: 230570  |  5: 359415  |  6: 230564  |  7: 359410
 RX by ports:
 0: 359434  |  1: 230580  |  2: 359415  |  3: 230571  |  4: 359429  |  5: 230579  |  6: 359411  |  7: 230565
 CPU utilization:
 [0.0, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8]

API Reference
=============
.. toctree::
    :maxdepth: 2

    api/index

Client Utilities
================
.. toctree::
    :maxdepth: 2

    client_utils

Usage Examples
==============
.. toctree::
    :maxdepth: 2

    usage_examples


Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`


.. rubric:: Footnotes

.. [#f1] For more information on JSON-RPC, check out the `official site <http://www.jsonrpc.org/>`_
