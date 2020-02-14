Non Drop Rate
========================

The NDR's purpose is to identify the maximal point in terms of packet and bandwidth throughput at which the Packet Loss Ratio (PLR) stands at 0%.
T-Rex offers a benchmark to find the NDR point.
The benchmarker is meant to test different DUTs. Each DUT has its own capabilities, specifications and API.
We clearly can't support all the DUTs in the world.
Hence, we offer the user a plugin API which can help him integrate his DUT with the TRex NDR Benchmarker. 
The user can control his device before each iteration (pre) and optimize his DUT for maximum performance.
He also can get different data from the DUT after (post) the iteration, and decide whether to continue to the next iteration or stop.

Also, we offer the user the API of the benchmarker. In case someone wants to modify our implementation for their own needs, or create their own scripts that wrap the benchmark.

The NDR script is implemented for STL and ASTF. This modes are different and finding the NDR in each mode is approached differently. Hence consult the API of the mode you are interested in.


`Complete Documentation <https://trex-tgn.cisco.com/trex/doc/trex_ndr_bench_doc.html>`_


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

