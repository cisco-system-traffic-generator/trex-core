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

* **TODO**  Yaroslav Client zip as self-contain

How to pyATS/v2.0
==================

.. sectionauthor:: David Shen 

pyATS Compatibility 

Trex only supports python2 for now, so it only works for **Python2** pyats.
     
* Install python2 pyats
  	/auto/pyats/bin/pyats-install --python2

* setenv TREX_PATH to the trex stateless lib path 
   	setenv TREX_PATH <your path>/automation/trex_control_plane/stl

* In the script or job file, add the TREX_PATH to sys.path::
 
 	import sys, os; sys.path.append(os.environ['TREX_PATH'])

* Source trex stateless libs in scripts::

          from trex_stl_lib.api import *
          from scapy.contrib.mpls import *
          from trex_stl_lib.trex_stl_hltapi import *


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

