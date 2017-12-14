
Usage Examples
==============


Full-featured interactive shell
-------------------------------

The `client_interactive_example.py` extends and uses the `Cmd <https://docs.python.org/2/library/cmd.html>`_ built in python class to create a Full-featured shell using which one can interact with TRex server and get instant results.

The help menu of this application is:

.. code-block:: python

        usage: client_interactive_example [options]

        Run TRex client API demos and scenarios.

        optional arguments:
          -h, --help            show this help message and exit
          -v, --version         show program's version number and exit
          -t HOST, --trex-host HOST
                                Specify the hostname or ip to connect with TRex
                                server.
          -p PORT, --trex-port PORT
                                Select port on which the TRex server listens. Default
                                port is 8090.
          -m SIZE, --maxhist SIZE
                                Specify maximum history size saved at client side.
                                Default size is 100.
          --verbose             Switch ON verbose option at TRex client. Default is:
                                OFF.

**Code Excerpt**

.. literalinclude:: ../examples/client_interactive_example.py
   :language: python
   :emphasize-lines: 0
   :linenos:


End-to-End cycle
----------------

This example (``pkt_generation_for_trex.py``) demonstrates a full cycle of using the API.

.. note:: this module uses the `Scapy <http://www.secdev.org/projects/scapy/doc/usage.html>`_ in order to generate packets to be used as a basis of the traffic injection. It is recommended to *install* this module to best experience the example.

The demo takes the user a full circle: 
    1. Generating packets (using Scapy)
    2. exporting the generated packets into .pcap file named `dns_traffic.pcap`.
    3. Use the :class:`trex_yaml_gen.CTRexYaml` generator to include that pcap file in the yaml object.
    4. Export the YAML object onto a YAML file named `dns_traffic.yaml`
    5. Push the generated files to TRex server.
    6. Run TRex based on the generated (and pushed) files.

**Code Excerpt** [#f1]_ 

.. literalinclude:: ../examples/pkt_generation_for_trex.py
   :language: python
   :lines: 10-
   :emphasize-lines: 32,36,42,46,51,60,63-69,76-80
   :linenos:


.. rubric:: Footnotes

.. [#f1] The marked codelines corresponds with the steps mentioned above.
