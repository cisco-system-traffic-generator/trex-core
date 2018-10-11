.. TRex Stateless Python API documentation 
   contain the root `toctree` directive.

TRex Stateless Python API
==============================================

TRex is a **traffic generator** 

This site covers the Python API of TRex and explains how to utilize it to your needs.
To understand the entirely how the API works and how to set up the server side, check out the `trex-core Wiki <https://github.com/cisco-system-traffic-generator/trex-core/wiki>`_ under the documentation section of TRex website.
   
**Use the table of contents below or the menu to your left to navigate through the site**

How to Install 
===============
.. toctree::
    :maxdepth: 2

| TRex package contains trex_client.tar.gz
| Put it at any place you like, preferably same place as your scripts.
| (If it's not at same place as your scripts, you will need to ensure trex_client directory is in sys.path)

Un-pack it using command::

 tar -xzf trex_client.tar.gz
 
How to use
==================

| The client assumes server is running.
| After un-tarring the client package, you can verify basic tests in examples directory out of the box:

.. code-block:: bash

 cd trex_client/stl/examples
 python stl_imix.py -s <server address>

You can verify that the traffic was sent and arrives properly if you see something like this:::

 Mapped ports to sides [0, 2] <--> [1, 3]
 Injecting [0, 2] <--> [1, 3] on total rate of '30%' for 10 seconds

 Packets injected from [0, 2]: 473,856
 Packets injected from [1, 3]: 473,856

 packets lost from [0, 2] --> [0, 2]:   0 pkts
 packets lost from [1, 3] --> [1, 3]:   0 pkts

 Test has passed :-)


Also, in the stl folder there are directories with profiles that define streams and the console (with which you can easily send the profiles)

How to pyATS
==================

.. sectionauthor:: David Shen 

pyATS Compatibility 

TRex supports both Python2 and Python3 pyATS.
     
* Install python2/python3 pyats
  	/auto/pyats/bin/pyats-install --python2
  	/auto/pyats/bin/pyats-install --python3

* setenv TREX_PATH to the trex stateless lib path 
   	setenv TREX_PATH <your path>/automation/trex_control_plane/interactive/trex/stl

* In the script or job file, add the TREX_PATH to sys.path::
 
 	import sys, os; sys.path.append(os.environ['TREX_PATH'])

* Source trex stateless libs in scripts::

          from trex.stl.api import *
          from scapy.contrib.mpls import *
          from trex.stl.trex_stl_hltapi import *

If using trex_client package, import syntax is::

    from trex_client.stl.api import *


API Reference
==============
.. toctree::
    :maxdepth: 2

    api/index

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`


.. rubric:: Footnotes

